/*                  
 *        File: InF_StochasticSP.cc
 *      Author: Kanishk Chauhan
 *        Date: Dec 2023
 * Description: This code uses our stochastic SP method. Single contact from a presynaptic neuron to its given postsynaptic partner is allowed.
 *              It also saves the steady-state data in a file. This file can be used for stimulation elsewhere.
 */

# include <iostream>
# include <algorithm>
# include <iomanip>
# include <sstream>
# include <cmath>
# include <cstdlib>
# include <fstream> 
# include <vector>
# include <complex>
# include <random>

using namespace std;

// ----------------------TAKEN FROM NUMERICAL RECIPES FOR GENERATING RANDOM NUMBERS--------------------------------
struct Ran {
    typedef unsigned int long long Ullong; 
    typedef unsigned int Uint;
    Ullong u,v,w;
    Ran(Ullong j) : v(4101842887655102017LL), w(1) {
    u = j ^ v; int64();
    v = u; int64();
    w = v; int64();
}
inline Ullong int64() {
    u = u * 2862933555777941757LL + 7046029254386353087LL;
    v ^= v >> 17; v ^= v << 31; v ^= v >> 8;
    w = 4294957665U*(w & 0xffffffff) + (w >> 32);
    Ullong x = u ^ (u << 21); x ^= x >> 35; x ^= x << 4;
    return (x + v) ^ w;
}
inline double doub() { return 5.42101086242752217E-20 * int64(); } //Returns a double-type random number between 0 and 1
inline Uint int32() { return (Uint)int64(); } //Return 32-bit random integer.
};

struct Normaldev_BM : Ran {
    typedef double Doub;
    typedef unsigned int long long Ullong;
    Doub mu,sig;
    Doub storedval;
    Normaldev_BM(Doub mmu, Doub ssig, Ullong i) // constructor
    : Ran(i), mu(mmu), sig(ssig), storedval(0.) {}

    Doub dev() {
        Doub v1,v2,rsq,fac;
        if (storedval == 0.) {
            do {
                v1=2.0*doub()-1.0;
                v2=2.0*doub()-1.0;
                rsq=v1*v1+v2*v2;
            } while (rsq >= 1.0 || rsq == 0.0);
            fac=sqrt(-2.0*log(rsq)/rsq);
            storedval = v1*fac;
            return mu + sig*v2*fac;
        } else {
            fac = storedval;
            storedval = 0.;
            return mu + sig*fac;
        }
    }
};
// ------------------------------------------------------------------------------------------------------------------


// the function to generate Random Connectivity by Renyi-Erdos method
vector<vector<double> > Adjacency_Matrix(int N, double p);

// the functions to generate a the adjacency matrix with distance dependent connection probability  
vector<vector<double> > Dist_Dep_Adjacency_Matrix(int N, double p, vector<vector<double> > Zd); 
vector<vector<double> > Dist_Dep_Delta_Adjacency_Matrix(int N, double p, vector<vector<double> > Zd); // with delta distribution

// the function to update the neurons' membrane potential
void InF(double dt, int itrain, double gleak[], double gsyn[], double gnoise[], double noise[], double V[], double Vth[], double Istim[], double inlinks[], double train_pre[], 
                double tdelay, double Vrest, double Vsyn, double Vth_rest, double Tsyn, double Tth, double C0, double k, double knoise, int N, 
                vector<vector<double> > &A, vector<vector<double> > &W, vector<vector<int> > &Links);

// the function to implement the STDP rule with TRACE AND TRAIN METHOD
void STDP1(int N, double dt, double ed, double ep, double train_pre[], double train_post[], double trace_pre[], double trace_post[], double inlinks[], vector<vector<double> > &W, vector<vector<int> > &Links);

// the Structural Plasticity function
void SP(Ran &random, vector<vector<double> > &W, vector<vector<double> > &A, vector<vector<double> > &A0, vector<vector<double> > Zd, double fi[], double inlinks[], double Tw, double Wmin, double Ph, double Pw, double c, double f1, double f2, double nu, int &pruned, int &added, double &n_con);

const int m = 20;
const int N = m*m; // number of neurons

int main(int argc, char* argv[]){

    // for random numbers
    Ran random(100); //100 is given as a seed for random number generator struct. Now random.doub() would generate a uniform random number between 0 and 1
    Normaldev_BM normal_random(0,1,100); // mean = 0, s.d. = 1, and seed = 100. Now normal_random.dev() would generate a normally distributed random number generated by B-M method

    double Tw,tp,tr,td,t,dt,dt_inv,msecs,tmax,pi,d_theta,d_theta_wrapped,ed,ep,epsilon,sum,a,f0,gamma,D,hours,w_sd,w0,W_mean,W_sd,dn,nl,n_con,n_con_init,knoise,fnoise,Tth,Tsyn,k,Vsyn,Vrest,Vth_rest,C0,Cstd,Vreset,Vspike,Tspike,Vth_spike,tdelay,Tslow;
    double rnd, current_time,Avg_W,node_deg,order_par,rand_num,lamda,Pa,I,p,beta_min,beta_max,n_con_max,n_con_min,n_con_p,Fs,e,ki0,ki0N,ki0p,ki0Np,eta,tou_st,stim_sigma,b,H,sum_loc,ee,d0,lifetime_hole,P0,SI,Wmin,dd,L,dc,rtime0,fi_mean,fi_sd,spk;
    double gleak0, gleak_std, fT, fstd, odpr, sumsin, sumcos, hr, un, vn, dst, Pw, Ph, fi_mean0, Avg_W0, fi_err, Avg_Werr, max_err;
    int nt,ntrain,itrain,n_noise_spikes,i_noise_spike,num_itrn,pos,pre,np,pruned,added,index,Na,a_init_pos,a_final_pos,part,rtime,stime,pstime,stim_width,frac,osc_index,win_size,nspk,nlo,flg,sp_it,nSPupdate,iSPupdate;
    double V[N], V0[N], Vth[N], C[N], gleak[N], theta[N], fi[N], cpl_term[N], theta_new[N], gnoise[N], Istim[N], Inoise[N], noise[N], gsyn[N], flag[N], trace_pre[N];
    double trace_post[N], noise_train_array[N], inlinks[N], outlinks[N], osc_indices[N], avgW[N], spikeTimes[N], spikeTimes0[N], train_pre[N], train_post[N], phi[N];
    double xi[N], yi[N];
    int counts[N], counts2[N];
    double c1,f1,f2,lambda_h,lambda_w,delta_f,nu,p0; // variables for structural plasticity

    // command line inputs
    a = atof(argv[1]); // the asymmetry parameter of STDP
    W_mean = atof(argv[2]); // the initial mean synaptic weight of the network
    p = atof(argv[3]); // the initial average NDD

    // the files to store all the data
    char* filename1 = new char[500];
    char* filename2 = new char[500];
    char* filename3 = new char[500];
    char* filename4 = new char[500];
    char* filename5 = new char[500];

    sprintf(filename1,"STDP+rSP(a=%.1f,Wmean=%.1f,k=8,beta=%.3f).dat",a,W_mean,p);
    sprintf(filename2,"W_STDP+rSP(a=%.1f,Wmean=%.1f,k=8,beta=%.3f).dat",a,W_mean,p);
    sprintf(filename3,"A_STDP+rSP(a=%.1f,Wmean=%.1f,k=8,beta=%.3f).dat",a,W_mean,p);
    sprintf(filename4,"FR_STDP+rSP(a=%.1f,Wmean=%.1f,k=8,beta=%.3f).dat",a,W_mean,p);
    sprintf(filename5,"Steady_state_STDPrSP(a=%.1f,Wmean=%.1f,k=8,beta=%.3f).dat",a,W_mean,p);

    ofstream data(filename1);
    ofstream W_file(filename2);
    ofstream A_file(filename3);
    ofstream FR_file(filename4);
    ofstream steadystate(filename5);

    // time stuff
    nSPupdate = 2000; // number of maximum SP updates allowed. 
    msecs = 3600e3; // number of miliseconds in 1 hour
    dt = 0.5; // in ms
    dt_inv = 1/dt; // the height of a spike
    rtime0 = 600e3; // the relaxtion time without plasticity (in ms)
    Tw = 60e3; 
    nt = Tw/dt; // number of time steps in one Tw long window
    
    // connectivity constraints
    fT = 4.5; // Target firing rate in Hz
    f0 = 3.0; // network-averaged firing rate in units of spike/time
    fstd = 0.5; // allowed standard deviaiton in the uncoupled neurons' firing rates
    d0 = 1.0/(N-1); // for stimulus spatial profile
    
    // parameter values
    k = 8.0; // in mS/cm^2
    C0 = 3; // mean neuron membrane capacitance in units of micro F/cm^2
    gleak0 = (f0 - 0.9177303000633497)/125.6691632725962;  // these 'm' and 'c' for fr=m*gleak+c will change with noise intensity.
    gleak_std = fstd/125.6691632725962;
    Vrest = -38.0; // resting potential in mV
    Vreset = -67.0; // in mV
    Vsyn = 0.0; // in mV
    Vth_spike = 0.0; // in mV
    Vth_rest = -40.0; // in mV
    Vspike = 20.0; // in mV
    Tspike = 1.0; // in ms
    Tsyn = 1.0; // in ms
    Tth = 5.0; // in ms
    tdelay = 3.0; // in ms
    Tslow = 30e3; // in ms
    knoise = 0.06; // in mS/cm^2
    fnoise = 20; // in Hz
    tr = 4.0; // scales the depression time constant wrt potentiation
    tp = 10.0; // in ms
    td = tr*tp;
    eta = 0.02; // weight change quantum
    ep = eta;
    ed = a*eta/tr; // As in Kromer and Tass's PRR 2020 Paper (Long-lasting desynchronization by decoupling stimulus)
    W_sd = 0.05; // standard deviation of weights
    win_size = 10; // 5 on the right and 5 on the left
    if(win_size%2==0) win_size+=1; // to keep the window size odd
    pi = 4*atan(1);
    max_err = 1e-3;
    
    // Structural plasticity parameters
    Wmin = 0.001; // for weight dependent pruning
    delta_f = 1.0; // it also controls the steepness parameter 'nu'
    f1 = fT - delta_f/2;
    f2 = fT + delta_f/2;
    c1 = exp(-1.0/Wmin);
    p0 = 0.01; // fraction of homeostatic addition and pruning probability when a neuron's firing rate equals the target rate
    nu = delta_f/(2*log((1-p0)/p0)); // the steepness controlling parameter of the logistic function
    Pw = 0.01; // weight dependent prunign probability
    Ph = 0.01; // homeostatic SP probability

    // positions of and distance between neurons
    hr = 1.0/(m-1); // m divisions in 1.0mm
    dst = 0.5; // in mm
    vector<double> zerodis(N,0);
    vector<vector<double> > dij(N,zerodis);
    vector<vector<double> > Zd(N,zerodis);
    for (int i = 0; i < N; i++){
        un = random.doub(); vn = random.doub();
        xi[i] = fmod((i+0.5),m)*hr + hr/10.0 * sqrt(-2*log(un)) * cos(2*pi*vn); // x-position of the i-th neuron
        yi[i] = (0.5+int(i/m))*hr + hr/10.0 * sqrt(-2*log(un)) * sin(2*pi*vn); // y-position of the i-th neuron
    }
    for (int i = 0; i < N; i++){
        for (int j = 0; j < N; j++){
            dij[i][j] = sqrt(pow((xi[i]-xi[j]),2)+pow((yi[i]-yi[j]),2));
            Zd[i][j] = exp(-dij[i][j]/dst);
        }
    }

    // the 2D 'spike train' array- rows= neurons, columns= corresponding spike trains
    ntrain = int(tdelay/dt); if(ntrain == 0) ntrain = 1; // number of points we need in the spike trains of all neurons
    vector<double> seero(ntrain,0);
    vector<vector<double> > train(N,seero);

    // the 2D 'inter-spike interval' array- rows= neurons, columns= corresponding inter-spike intervals
    vector<double> zeroisi(nt,0);
    vector<vector<double> > isi(N,zeroisi);
    
    // the 'Adjacency matrix' and its copy for correct structural updates
    vector<double> zeero(N,0);
    vector<vector<double> > A(N,zeero);
    A = Dist_Dep_Adjacency_Matrix(N,p,Zd); // the initial adjacency matrix
    vector<vector<double> > A0 = A; 

    // the 2D array to store the presynaptic partner information (neuron number) for all neurons
    vector<int> sero(N,0);
    vector<vector<int> > Links(N,sero);

    // initial values of Capacitance, membrane potential, threshold, etc
    for(int i = 0; i < N; i++){
        gleak[i] = gleak0+gleak_std*normal_random.dev();
        V[i] = Vreset + (Vrest-Vreset)*random.doub();//cout << V[i] << " " ;// -67 + (-38+67)*random(between 0 and 1)
        fi[i] = 0;
        Vth[i] = Vth_rest;
        gsyn[i] = 0;
        gnoise[i] = 0;
        flag[i] = 1;
        spikeTimes[i] = 0;
        train_pre[i] = train_post[i] = trace_pre[i] = trace_post[i] = 0;
        osc_indices[i] = i;
        Istim[i] = 0;
        counts2[i] = 0;
    }

    // initial values of Wij (elements of the weight matrix)
    vector<double> zeros(N,0);
    vector<vector<double> > W(N,zeros);
    for (int ii = 0; ii < N; ii++){
        for (int jj = 0; jj < N; jj++){
            if (A[ii][jj] == 1) W[ii][jj] = W_mean + W_sd*(2*random.doub()-1); // random values-> W_mean +- W_sd 
        }
    }

    // recording the initial weights
    for(int ii = 0; ii < N; ii++){
        for(int jj = 0; jj < N; jj++){
            W_file << A[ii][jj]*W[ii][jj] << "  ";
        }
        W_file << endl;
    }
    // recording the initial Adjacency Matrix
    for(int ii = 0; ii < N; ii++){
        for(int jj = 0; jj < N; jj++){
            A_file << A[ii][jj] << "  ";
        }
        A_file << endl;
    }

    // for the 2D 'noise spike time intervals' array- rows= neurons, columns= corresponding noise inter-spike intervals
    n_noise_spikes = int(10*Tw/1000*fnoise); //cout << n_noise_spikes << endl;
    
    // calculating the INITIAL mean of the synaptic weights and node degree
    sum = 0; n_con = 0;
    for(int ii = 0; ii < N; ii++){
        nl = 0; pos = 0; sum_loc = 0; nlo = 0;
        for(int jj = 0; jj < N; jj++){
            if (A[ii][jj] == 1){
                sum_loc += W[ii][jj];
                Links[ii][pos] = jj;
                nl++;
                pos++;
            }
            if (A[jj][ii] == 1) nlo++;
        }
        outlinks[ii] = nlo;
        inlinks[ii] = nl;
        n_con += nl;
        sum += sum_loc;
        avgW[ii] = sum_loc/nl;
    }
    n_con_init = n_con; // this should not be updated after this step
    Avg_W = sum/n_con;
    node_deg = n_con/N/(N-1); 

//*********************************************SOME TIME WITHOUT PLASTICITY**********************************************//
    num_itrn = rtime0/Tw; //cout << "nt" << num_itrn << endl;
    for (int i = 0; i < num_itrn; i++){
        
        k = 0.0; // out of 'num_itrn' iterations without plasticity, first half are executed without coupling to let the neurons reach their stable natural firing rate
        if(i >= num_itrn/2) k = 8.0;
        for(int ii = 0; ii < N; ii++) counts[ii] = 0;
        odpr = 0;
        
        // the 2D 'noise train' array- rows= neurons, columns= corresponding noise spike trains
        vector<double> zeronoise(nt,0);
        vector<vector<double> > noise_train(N,zeronoise);
        // preparing the noise spike train using the exponentially distributed random inter-spike intervals
        for (int ii = 0; ii < N; ii++){ // for each neuron
            i_noise_spike = 0; // index of spike in the noise spike train
            for (int jj = 0; jj < n_noise_spikes; jj++){ 
                rnd = -log(random.doub())/fnoise; 
                i_noise_spike += int(rnd*1000/dt);
                if (i_noise_spike >= nt) break;
                noise_train[ii][i_noise_spike] = dt_inv;
            }
        }
        
        // for each time step of the iteration
        for (int j = 0; j < nt; j++){
            current_time = i*Tw + j*dt;
            itrain = int(j%ntrain); // the index for time 'tdelay' before the current time 

            // presynaptic train and trace
            for (int ii = 0; ii < N; ii++){ // for each neuron
                train_pre[ii] = train[ii][itrain]; // delayed spike train
                trace_pre[ii] = trace_pre[ii] + dt*(-trace_pre[ii]/tp + train_pre[ii]); // updating the presynaptic trace with the spike train 'tdelay' before the current time
            }

            // integrating the Integrate and Fire model equations
            for (int ii = 0; ii < N; ii++){
                V0[ii] = V[ii];
                noise[ii] = noise_train[ii][j];
            }
            InF(dt,itrain,gleak,gsyn,gnoise,noise,V,Vth,Istim,inlinks,train_pre,tdelay,Vrest,Vsyn,Vth_rest,Tsyn,Tth,C0,k,knoise,N,A,W,Links);
            
            // discriminating the spikes
            for (int ii = 0; ii < N; ii++){ // for each neuron
                train[ii][itrain] = 0;  // the train gets updated in this loop. Upto the step before this, the train is from 'tdelay' before the current time
                if (V[ii] >= Vth[ii] && flag[ii] == 1){
                    spikeTimes0[ii] = spikeTimes[ii];
                    spikeTimes[ii] = current_time;
                    isi[ii][counts[ii]] = spikeTimes[ii] - spikeTimes0[ii];
                    counts[ii] += 1; counts2[ii] += 1;
                    train[ii][itrain] = dt_inv;
                    Vth[ii] = Vth_spike;
                    V[ii] = Vspike;
                    flag[ii] = 0;
                }
                if (flag[ii] == 0){
                    if (j < (spikeTimes[ii] - i*Tw + Tspike)/dt){ // generating a rectangular spike of width Tspike
                        V[ii] = Vspike; 
                        Vth[ii] = Vth_spike;
                    }
                    else{
                        V[ii] = Vreset;
                        flag[ii] = 1;
                    }
                }
            }
            
            // postsynaptic train and trace
            for (int ii = 0; ii < N; ii++){ // for each neuron
                train_post[ii] = train[ii][itrain]; // current (non-delayed) spike train for postsynaptic neuron
                trace_post[ii] = trace_post[ii] + dt*(-trace_post[ii]/td + train_post[ii]); // updating the postsynaptic trace with the spike train at the current time
            }

            // the firing rate
            for (int ii = 0; ii < N; ii++){
                fi[ii] += dt/Tslow * (-fi[ii] + train_post[ii]*1e3);
            }

            // order parameter
            if (i > 0){
                sumsin = 0; sumcos = 0;
                for (int ii = 0; ii < N; ii++){
                    phi[ii] = 2*pi*((current_time - spikeTimes0[ii])/(spikeTimes[ii] - spikeTimes0[ii]) + counts2[ii]);
                    sumcos += cos(phi[ii]); sumsin += sin(phi[ii]);
                }
                odpr += sqrt(pow((sumcos/N),2) + pow((sumsin/N),2));
            }
        }
        odpr = odpr/nt; // time-averaging the order paramter

        fi_mean = 0;
        for (int ii = 0; ii < N; ii++) fi_mean += fi[ii]/N; // mean firing rate
        
        // calculating the mean of the synaptic weights and node degree
        sum = 0;
        for(int ii = 0; ii < N; ii++){
            nl = inlinks[ii]; sum_loc = 0;
            for(int jj = 0; jj < nl; jj++){
                pre = Links[ii][jj];
                sum_loc += W[ii][pre];
            }
            sum += sum_loc;
            avgW[ii] = sum_loc/nl;
        }
        Avg_W = sum/n_con;
        node_deg = n_con/N/(N-1);

        // recording the firing rates
        for(int ii = 0; ii < N; ii++){
            FR_file << fi[ii] << "  ";
        }
        FR_file << endl;
        
        data << left << setw(15) << odpr << left << setw(15) << fi_mean << left << setw(15) << Avg_W << left << setw(15) << node_deg << left << setw(15) << pruned << left << setw(15) << added<< left << setw(15) << 0 << endl; 
        cout << "Without plasticity: " << odpr << "  " << fi_mean << "   " << Avg_W << "   " << node_deg << "   " << pruned << "  " << added << endl;  
    }
    
//********************************************* transition to steady state with STDP and SP ************************************************//
    iSPupdate = 0;
    k = 8.0;
    flg = 2;
    int i = 0;
    while (iSPupdate < nSPupdate) { 
        pruned = 0;
        added = 0;
        odpr = 0;
        for(int ii = 0; ii < N; ii++) counts[ii] = 0;
        
        // the 2D 'noise train' array- rows= neurons, columns= corresponding noise spike trains
        vector<double> zeronoise(nt,0);
        vector<vector<double> > noise_train(N,zeronoise);
        // preparing the noise spike train using the exponentially distributed random inter-spike intervals
        for (int ii = 0; ii < N; ii++){ // for each neuron
            i_noise_spike = 0; // index of spike in the noise spike train
            for (int jj = 0; jj < n_noise_spikes; jj++){ 
                rnd = -log(random.doub())/fnoise; // exp_rnd(rnd_generator);
                i_noise_spike += int(rnd*1000/dt);
                if (i_noise_spike >= nt) break;
                noise_train[ii][i_noise_spike] = dt_inv;
            }
        }
        
        // for each time step of the iteration
        for (int j = 0; j < nt; j++){
            current_time = rtime0 + i*Tw + j*dt;
            itrain = int(j%ntrain); // the index for time 'tdelay' before the current time 

            // presynaptic train and trace
            for (int ii = 0; ii < N; ii++){ // for each neuron
                train_pre[ii] = train[ii][itrain]; // delayed spike train
                trace_pre[ii] = trace_pre[ii] + dt*(-trace_pre[ii]/tp + train_pre[ii]); // updating the presynaptic trace with the spike train 'tdelay' before the current time
            }

            // integrating the Integrate and Fire model equations
            for (int ii = 0; ii < N; ii++){
                V0[ii] = V[ii];
                noise[ii] = noise_train[ii][j];
            }
            InF(dt,itrain,gleak,gsyn,gnoise,noise,V,Vth,Istim,inlinks,train_pre,tdelay,Vrest,Vsyn,Vth_rest,Tsyn,Tth,C0,k,knoise,N,A,W,Links);
            
            // discriminating the spikes
            for (int ii = 0; ii < N; ii++){ // for each neuron
                train[ii][itrain] = 0;  // the train gets updated in this loop. Upto the step before this, the train is from 'tdelay' before the current time
                if (V[ii] >= Vth[ii] && flag[ii] == 1){
                    spikeTimes0[ii] = spikeTimes[ii];
                    spikeTimes[ii] = current_time;
                    isi[ii][counts[ii]] = spikeTimes[ii] - spikeTimes0[ii];
                    counts[ii] += 1; counts2[ii] += 1;
                    train[ii][itrain] = dt_inv;
                    Vth[ii] = Vth_spike;
                    V[ii] = Vspike;
                    flag[ii] = 0;
                }
                if (flag[ii] == 0){
                    if (j < (spikeTimes[ii] - rtime0 - i*Tw + Tspike)/dt){ // generating a rectangular spike of width Tspike
                        V[ii] = Vspike; 
                        Vth[ii] = Vth_spike;
                    }
                    else{
                        V[ii] = Vreset;
                        flag[ii] = 1;
                    }
                }
            }
            // postsynaptic train and trace
            for (int ii = 0; ii < N; ii++){ // for each neuron
                train_post[ii] = train[ii][itrain]; // current (non-delayed) spike train for postsynaptic neuron
                trace_post[ii] = trace_post[ii] + dt*(-trace_post[ii]/td + train_post[ii]); // updating the postsynaptic trace with the spike train at the current time
            }

            // the firing rate
            for (int ii = 0; ii < N; ii++){
                fi[ii] += dt/Tslow * (-fi[ii] + train_post[ii]*1e3);
            }

            // order parameter
            sumsin = 0; sumcos = 0;
            for (int ii = 0; ii < N; ii++){
                phi[ii] = 2*pi*((current_time - spikeTimes0[ii])/(spikeTimes[ii] - spikeTimes0[ii]) + counts2[ii]);
                sumcos += cos(phi[ii]); sumsin += sin(phi[ii]);
            }
            odpr += sqrt(pow((sumcos/N),2) + pow((sumsin/N),2));
            
            STDP1(N,dt,ed,ep,train_pre,train_post,trace_pre,trace_post,inlinks,W,Links); // updating the weights
           
        }
        odpr = odpr/nt; // time-averaging the order paramter

        fi_mean0 = fi_mean;
        fi_mean = 0;
        for (int ii = 0; ii < N; ii++) fi_mean += fi[ii]/N;
        fi_err = 2*abs(fi_mean-fi_mean0)/(fi_mean+fi_mean0); // relative error between current and previous network-averaged firing rates 
        
        Avg_W0 = Avg_W;
        // calculating the mean of the synaptic weights and node degree
        sum = 0; n_con = 0;
        for(int ii = 0; ii < N; ii++){
            nl = inlinks[ii]; sum_loc = 0;
            for(int jj = 0; jj < nl; jj++){
                pre = Links[ii][jj];
                sum_loc += W[ii][pre];
            }
            n_con += nl;
            sum += sum_loc;
            avgW[ii] = sum_loc/nl;
        }
        Avg_W = sum/n_con;
        node_deg = n_con/N/(N-1);
        Avg_Werr = 2*abs(Avg_W-Avg_W0)/(Avg_W+Avg_W0); // relative error between current and previous network-averaged weight 

        // recording the firing rates every 10th iteration until the SP updates begin. After that, it is recorded at each SP update 
        if (i % 10 == 0 && flg == 2){
            for(int ii = 0; ii < N; ii++){
                FR_file << fi[ii] << "  ";
            }
            FR_file << endl;
        }

        if (i > 1*msecs/Tw && Avg_Werr < max_err && fi_err < max_err && flg == 2){ // at least for the first 1 hour, there is no structural update
            flg = 0;
            // break; // uncomment if only STDP is desired, not SP
        }
        if ( ((Avg_Werr < max_err && fi_err < max_err) || i%30 == 0) && flg == 0){ 
            flg = 1;
            sp_it = i;
        } 
        if (i == sp_it+1 && flg == 1){
            SP(random,W,A,A0,Zd,fi,inlinks,Tw,Wmin,Ph,Pw,c1,f1,f2,nu,pruned,added,n_con);
            flg = 0;
            iSPupdate += 1;
            for(int ii = 0; ii < N; ii++){
                FR_file << fi[ii] << "  ";
            }
            FR_file << endl;
        }

        // updating the in-links and calculating the out-node degree (# of post-synaptic partners- it's just the column-wise sum of A)
        for(int ii = 0; ii < N; ii++){
            pos = 0; nlo = 0;
            for(int jj = 0; jj < N; jj++){
                if (A[ii][jj] == 1){
                    Links[ii][pos] = jj;
                    pos++;
                }
                if (A[jj][ii] == 1) nlo++;
            }
            outlinks[ii] = nlo;
        }
        
        data << left << setw(15) << odpr << left << setw(15) << fi_mean << left << setw(15) << Avg_W << left << setw(15) << node_deg << left << setw(15) << pruned << left << setw(15) << added << left << setw(15) << iSPupdate << endl;
        cout << odpr << "  " << fi_mean << "   " << Avg_W << "   " << node_deg << "   " << pruned << "  " << added << endl;  
        i += 1;
    }

    // recording the weights
    for(int ii = 0; ii < N; ii++){
        for(int jj = 0; jj < N; jj++){
            W_file << A[ii][jj]*W[ii][jj] << "  ";
        }
        W_file << endl;
    }

    // recording the Adjacency Matrix
    for(int ii = 0; ii < N; ii++){
        for(int jj = 0; jj < N; jj++){
            A_file << A[ii][jj] << "  ";
        }
        A_file << endl;
    }

    // recording the final steady state
    for(int ii = 0; ii < N; ii++) steadystate << xi[ii] << endl;
    for(int ii = 0; ii < N; ii++) steadystate << yi[ii] << endl;
    for(int ii = 0; ii < N; ii++) steadystate << gleak[ii] << endl;
    for(int ii = 0; ii < N; ii++) steadystate << V[ii] << endl;
    for(int ii = 0; ii < N; ii++) steadystate << Vth[ii] << endl;
    for(int ii = 0; ii < N; ii++) steadystate << fi[ii] << endl;
    for(int ii = 0; ii < N; ii++) steadystate << gsyn[ii] << endl;
    for(int ii = 0; ii < N; ii++) steadystate << gnoise[ii] << endl;
    for(int ii = 0; ii < N; ii++) steadystate << flag[ii] << endl;
    for(int ii = 0; ii < N; ii++) steadystate << train_pre[ii] << endl;
    for(int ii = 0; ii < N; ii++) steadystate << train_post[ii] << endl;
    for(int ii = 0; ii < N; ii++) steadystate << trace_pre[ii] << endl;
    for(int ii = 0; ii < N; ii++) steadystate << trace_post[ii] << endl;
    for(int ii = 0; ii < N; ii++) for(int jj = 0; jj < N; jj++) steadystate << A[ii][jj] << endl; 
    for(int ii = 0; ii < N; ii++) for(int jj = 0; jj < N; jj++) steadystate << A[ii][jj]*W[ii][jj] << endl;

    W_file.close();
    A_file.close();
    FR_file.close();
    data.close();
    steadystate.close();
    return 0;
}


vector<vector<double> > Adjacency_Matrix(int N, double p){ //Erdos Renyi random graph
    double rnd;
    vector<double> zeros(N,0);
    vector<vector<double> > matrix(N,zeros);
    for (int i = 0; i < N; i++){
        for (int j = 0; j < N; j++){
            if(i == j) continue;
            rnd = 0.0001*(rand()%10001);
            if(rnd <= p){
                matrix[i][j] = 1;
            }
        }
    }
    return matrix;
}

vector<vector<double> > Dist_Dep_Adjacency_Matrix(int N, double p, vector<vector<double> > Zd){
    double ncon=0;
    double rnd;
    vector<double> zeros(N,0);
    vector<vector<double> > matrix(N,zeros);
    while (p-ncon/N/(N-1) > 0.001){ 
        for (int i = 0; i < N; i++){
            for (int j = 0; j < N; j++){
                rnd = 0.0001*(rand()%10001);
                if (i!=j && matrix[i][j] == 0 && rnd < p*Zd[i][j]){
                    matrix[i][j] = 1;
                    ncon += 1;
                }
            }
            if (ncon > p*N*(N-1)) break;
        }
    }        
    return matrix;
}

vector<vector<double> > Dist_Dep_Delta_Adjacency_Matrix(int N, double p, vector<vector<double> > Zd){ // generates a network with delta distribution of degrees with distance-dependent connectivity
    double ncon;
    double rnd;
    vector<double> zeros(N,0);
    vector<vector<double> > matrix(N,zeros);
    for (int i = 0; i < N; i++){
        ncon = 0;
        while (ncon < p*(N-1)){
            for (int j = 0; j < N; j++){
                rnd = 0.0001*(rand()%10001);
                if (i!=j && matrix[i][j] == 0 && rnd < p*Zd[i][j]){
                    matrix[i][j] = 1;
                    ncon += 1;
                }
                if (ncon >= p*(N-1)) break;
            }
        }
    }            
    return matrix;
}

void InF(double dt, int itrain, double gleak[], double gsyn[], double gnoise[], double noise[], double V[], double Vth[], double Istim[], double inlinks[], double train_pre[], 
                double tdelay, double Vrest, double Vsyn, double Vth_rest, double Tsyn, double Tth, double C0, double k, double knoise, int N, 
                vector<vector<double> > &A, vector<vector<double> > &W, vector<vector<int> > &Links){
    
    double cpl_term;
    int nl, pre;
    for(int ii = 0; ii < N; ii++){ // updating the membrane potential and the threshold of each neuron
        V[ii] = V[ii] + dt/C0 * (gleak[ii]*(Vrest - V[ii]) + (gsyn[ii] + gnoise[ii])*(Vsyn - V[ii]) + Istim[ii]);
        Vth[ii] = Vth[ii] - dt/Tth * (Vth[ii] - Vth_rest);
    }
    for(int ii = 0; ii < N; ii++){ // updating the 'gsyn' of each neuron
        cpl_term = 0;
        nl = inlinks[ii];
        for (int jj = 0; jj < nl; jj++){ // evaluating the coupling term
            pre = Links[ii][jj];
            cpl_term += W[ii][pre] * train_pre[pre]; // train_pre[i] is either 0 or 1/dt
        }
        gsyn[ii] = gsyn[ii] + dt/Tsyn * (-gsyn[ii] + k*Tsyn*cpl_term/N);
    }
    for(int ii = 0; ii < N; ii++){ // updating the 'gnoise' of each neuron
        gnoise[ii] = gnoise[ii] + dt/Tsyn * (-gnoise[ii] + knoise*Tsyn*noise[ii]);
    }
     
}

void STDP1(int N, double dt, double ed, double ep, double train_pre[], double train_post[], double trace_pre[], double trace_post[], double inlinks[], vector<vector<double> > &W, vector<vector<int> > &Links){
    int nl,pre;
    for (int ii= 0; ii< N; ii++){ // for each postsynaptic neuron
        nl = inlinks[ii];
        for (int jj = 0; jj < nl; jj++){
            pre = Links[ii][jj];
            W[ii][pre] = W[ii][pre] + dt*(-ed*(trace_post[ii]*train_pre[pre]) + ep*(trace_pre[pre]*train_post[ii]));
            if (W[ii][pre] > 1) W[ii][pre] = 1;
            if (W[ii][pre] < 0) W[ii][pre] = 0;
        } 
    }
}

void SP(Ran &random, vector<vector<double> > &W, vector<vector<double> > &A, vector<vector<double> > &A0, vector<vector<double> > Zd, double fi[], double inlinks[], double Tw, double Wmin, double Ph, double Pw, double c, double f1, double f2, double nu, int &pruned, int &added, double &n_con){
    double a,b,lambda_prn_h,G;
    int nl;

    // how A looked an itrn of structural update before
    for (int ii= 0; ii< N; ii++){
        for (int jj = 0; jj < N; jj++){
            A0[ii][jj] = A[ii][jj]; // 1 structural update before
        }
    }

    // pruning
    for (int ii= 0; ii< N; ii++){ // for each oscillator
        nl = inlinks[ii];
        for (int jj = 0; jj < N; jj++){
            if (A[ii][jj] == 1 && nl > 2 && (random.doub() <  (Pw * exp(-W[ii][jj]/Wmin) +  Ph/(1+exp(-(fi[ii]-f2)/nu))) )){ 
                A[ii][jj] = 0;
                W[ii][jj] = 0;
                pruned++; n_con--; nl--;
            }
        } 
        inlinks[ii] = nl;
    }
    
    // adding
    for (int ii= 0; ii< N; ii++){ // for each oscillator
        nl = inlinks[ii];
        for (int jj = 0; jj < N; jj++){
            if (A0[ii][jj] == 0 && ii != jj && random.doub() < Ph/(1+exp((fi[ii]-f1)/nu)) * Zd[ii][jj]){
                A[ii][jj] = 1;
                W[ii][jj] = 0.2*random.doub();
                added++; n_con++; nl++;
            }
        }
        inlinks[ii] = nl;
    }
}