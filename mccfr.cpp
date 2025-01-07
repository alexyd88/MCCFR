#include <bits/stdc++.h>
#include <ctime>
#include <cstdlib>
#include <algorithm>
#include <chrono> 

using namespace std;

// put money in, don't put money in
const int NUM_MOVES = 2;
const double ACCEPTABLE_ERROR = 0.025;
const int SAMPLES = 100;
const int MAX_EPOCHS = 10000000;
map<string, array<double, NUM_MOVES>> strat;
map<string, array<double, NUM_MOVES>> regrets;
map<string, array<double, NUM_MOVES>> sum_strat;
map<string, array<double, NUM_MOVES>> avg_strat;
string deck = "JQK";
bool verbose = false;
bool CFR_PLUS = false;
bool LCFR = false;

void reset() {
    strat.clear();
    sum_strat.clear();
    avg_strat.clear();
    regrets.clear();
}

string p1_infoset(string history) {
    string res = history;
    for (int i = 1; i < history.size(); i += 2)
        res[i] = '?';
    return res;
}

string get_p1_infoset(string history) {
    string res = history;
    res[1] = '?';
    return res;
}

string get_p2_infoset(string history) {
    string res = history;
    res[0] = '?';
    return res;
}

string get_infoset(string history, bool am_player1) {
    if (am_player1)
        return get_p1_infoset(history);
    else
        return get_p2_infoset(history);
}

char action_to_char(int a) {
    if (a == 0)
        return 'b';
    return 'p';
}

void display_mapping(map<string, array<double, NUM_MOVES>> mp) {
    for (auto it = mp.begin(); it != mp.end(); it++) {
        cout << it->first << ": ";
        for (int i = 0; i < NUM_MOVES; i++) {
            cout << it->second[i] << " ";
        }
        cout << endl;
    }
    cout << endl;
}

void update_strat() {
    for (auto it = regrets.begin(); it != regrets.end(); it++) {
        bool is_one_regret_above_zero = false;
        double sum_regrets = 0;
        for (int i = 0; i < NUM_MOVES; i++) {
            if (it->second.at(i) > 0) {
                sum_regrets += it->second.at(i);
                is_one_regret_above_zero = true;
            }
            else {
                if (CFR_PLUS)
                    it->second[i] = 0;
            }
        }
            
        if (!is_one_regret_above_zero) {
            for (int i = 0; i < NUM_MOVES; i++)
                strat[it->first][i] = 1.0/NUM_MOVES;
            continue;
        }
        for (int i = 0; i < NUM_MOVES; i++) {
            if (it->second[i] > 0)
                strat[it->first][i] = it->second[i] / sum_regrets;
            else
                strat[it->first][i] = 0;
        }
    }
}

int get_action(string history) {
    // cout << "GETTING ACTION FOR " << history << endl;
    if (!strat.count(history)) {
        // cout << "NEVEr SEEN THIS SHIT " << history << endl;
        strat[history] = {};
        strat[history].fill(1.0/NUM_MOVES);
        return rand() % NUM_MOVES;
    }
    double x = (double)rand() / RAND_MAX;
    for (int i = 0; i < NUM_MOVES; i++) {
        x -= strat[history][i];
        if (x < 0)
            return i;
    }
    //cout << "SHOULD BE RARE ASF" << endl;
    return 0;
}

bool isWinner(char card1, char card2) {
    if (card1 == 'K')
        return true;
    if (card1 == 'Q' && card2 == 'J')
        return true;
    return false;
}

//returns 10 for in progress
int get_terminal_value(string history, bool am_player1) {
    char p1c = history[0];
    char p2c = history[1];
    int pot = 2;
    int p1balance = -1;
    int ansmultiple = (am_player1 ? 1 : -1);
    for (int i = 2; i < history.size(); i++) {
        if (history[i] == 'b') {
            if (i%2 == 0)
                p1balance--;
            pot++;
            if (pot == 4) {
                if (isWinner(p1c, p2c))
                    p1balance += pot;
                return ansmultiple * p1balance;
            }
        }

        if (history[i] == 'p') {
            if (pot == 3) {
                if (i%2)
                    p1balance += pot;
                return ansmultiple * p1balance;
            }
            if (i == 3) {
                if (isWinner(p1c, p2c))
                    p1balance += pot;
                return ansmultiple * p1balance;
            }
        }
    }

    return 10;
}

int is_terminal(string history) {
    //am player 1 shouldn't matter for checking if game is over
    return get_terminal_value(history, 0) != 10;
}

bool is_p1_turn(string history) {
    return history.size() % 2 == 0;
}

int get_sample_value(string history, bool am_player1) {
    if (is_terminal(history))
        return get_terminal_value(history, am_player1);
    history += action_to_char(get_action(get_infoset(history, is_p1_turn(history))));
    return get_sample_value(history, am_player1);
}
double get_strat_prob(string infostate, char action) {
    int ac = (action == 'p'); //pass is 1, bet is 0
    return avg_strat[infostate][ac];
}
bool within_acceptable_error(string infostate, char action, double b) {
    //cout << abs(get_strat_prob(infostate, action)-b) << endl;
    return abs(get_strat_prob(infostate, action)-b) < ACCEPTABLE_ERROR;
}

bool close_to_nash() {

    // chance p1 bluffs with J
    double alpha = get_strat_prob("J?", 'b');

    // p1 should raise K 3 times as often
    if (!within_acceptable_error("K?", 'b', alpha * 3))
        return false;

    // p1 should always call with K
    if (!within_acceptable_error("K?pb", 'b', 1))
        return false;

    // p1 always pass with Q
    if (!within_acceptable_error("Q?", 'p', 1))
        return false;

    // p1 should call raise after check with probability alpha + 1/3
    if (!within_acceptable_error("Q?pb", 'b', 1.0/3 + alpha))
        return false;

    // p2 should always bet with K
    if (!within_acceptable_error("?Kp", 'b', 1))
        return false;
    
    if (!within_acceptable_error("?Kb", 'b', 1))
        return false;

    //p2 should check with queen if possible
    if (!within_acceptable_error("?Qp", 'p', 1))
        return false;

    //p2 should call with queen 1/3 of the time
    if (!within_acceptable_error("?Qb", 'b', 1.0/3))
        return false;

    //p2 should never call with J
    if (!within_acceptable_error("?Jb", 'p', 1))
        return false;

    //p2 should bluff with J with probability 1/3
    if (!within_acceptable_error("?Jp", 'b', 1.0/3))
        return false;

    return true;
}

int train(std::mt19937 g) {
    bool am_player1 = true;
    int epochs;
    reset();
    for (epochs = 0; epochs < MAX_EPOCHS; epochs++) {
        update_strat();
        // cout << strat.size() << endl;
        if (verbose) {
            cout << "STRATEGY" << endl;
            display_mapping(strat);
        }
        for (auto j = strat.begin(); j != strat.end(); j++) {
            for (int k = 0; k < NUM_MOVES; k++)
                sum_strat[j->first][k] += j->second[k];
        }
        string history;
        shuffle(deck.begin(), deck.end(), g);
        history += deck[0];
        history += deck[1];
        //cout << deck[0] << " " << deck[1] << " " << deck[2] << endl;
        bool cam_player1 = true;
        while (!is_terminal(history)) {
            history += action_to_char(get_action(get_infoset(history, cam_player1)));
            cam_player1 = !cam_player1;
        }

        int utils = get_terminal_value(history, am_player1);
        if (verbose)
            cout << history << " " << am_player1 << endl;

        //cout << am_player1 << " " << history << " " << utils << endl;

        //go through and backtrack up to find better options :) :) :)
        for (int i = 2 + !am_player1; i <= history.size(); i+=2) {
            string prev_history = history.substr(0, i);
            string prev_infoset = get_infoset(prev_history, am_player1);
            // cout << "PREV HIST " << prev_history << endl;
            for (int j = 0; j < NUM_MOVES; j++) {
                int lcfrm = 1;
                if (LCFR)
                    lcfrm = epochs;
                regrets[prev_infoset][j] += (get_sample_value(
                    prev_history+action_to_char(j), am_player1)
                    - utils) * lcfrm;
            }
        }
        if (verbose) {
            cout << "REGRETS" << endl;
            display_mapping(regrets);
        }
        am_player1 = !am_player1;
        for (auto &i : sum_strat) {
            double sum = 0;
            for (int j = 0; j < i.second.size(); j++)
                sum += i.second[j];
            for (int j = 0; j < i.second.size(); j++)
                avg_strat[i.first][j] = i.second[j] / sum;
        }
        if (close_to_nash()) {
            break;
        }
    }
    if (epochs == MAX_EPOCHS) {
        cout << "shit" << endl;
    }
    // cout << fixed << setprecision(2);
    // cout << "FINAL STRAT" << endl;
    // display_mapping(strat);
    // cout << "FINAL REGRETS" << endl;
    // display_mapping(regrets);
    // cout << "AVERAGE STRAT" << endl;
    // display_mapping(avg_strat);

    // cout << "TOOK " << epochs << " EPOCHS TO CONVERGE" << endl;

    // cout << close_to_nash() << endl;
    return epochs;
}

int main() {
    std::random_device rd;
    std::mt19937 g(rd());
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    srand(seed);
    cout << fixed << setprecision(3);
    double sum_basic = 0;
    for (int i = 0; i < SAMPLES; i++) {
        sum_basic += train(g);
    }
    cout << "AVG FOR BASIC: " << sum_basic / SAMPLES << endl;
    CFR_PLUS = true;
    double sum_cfr_plus = 0;
    for (int i = 0; i < SAMPLES; i++) {
        sum_cfr_plus += train(g);
    }
    cout << "AVG FOR CFR PLUS: " << sum_cfr_plus / SAMPLES << endl;
    CFR_PLUS = false;
    LCFR = true;
    double sum_lcfr = 0;
    for (int i = 0; i < SAMPLES; i++) {
        sum_lcfr += train(g);
    }
    cout << "AVG FOR LCFR PLUS: " << sum_lcfr / SAMPLES << endl;
}