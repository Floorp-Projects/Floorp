# Parse the binary log file of activity timings.
#
# If run as a script, print a summary of the log, giving the total
# time in each activity.

import sys
import struct

from config import *
from acts import *

S_INTERP = 1
assert states[S_INTERP] == 'interpret'

REASON_COUNT = len(reasons)

def tuple_to_reason_index(rec):
    if rec[1]:
        return rec[1]
    return REASON_COUNT + rec[0]

def reason_index_label(i):
    if i == REASON_COUNT:
        return 'start'
    if i > REASON_COUNT:
        return states[i - REASON_COUNT]
    return reasons[i]

class History(object):
    def __init__(self):
        # list of (state, reason, time from start) tuples
        self.transitions = []
        # list of total time
        self.state_summary = [ 0 ] * len(states)
        # list of (count, total time) tuples
        self.reason_summary = [ [0, 0] for _ in range(len(reasons) + len(states)) ]

        self.t0 = None
        self.stack = []
        self.to_interp = None

    def write(self, state, reason, time):
        self.transitions.append((state, reason, time - self.t0))

    def transition(self, rec0, rec1):
        t1 = rec1[2]
        dt = rec1[2] - rec0[2]
        self.state_summary[rec0[0]] += dt

        self.write(rec0[0], 0, rec1[2])
        
        # Transition to interpreter
        if rec0[0] != S_INTERP and rec1[0] == S_INTERP:
            self.to_interp = (rec0[0], rec1[1], rec1[2])
        
        # Transition from interpreter
        if rec0[0] == S_INTERP and rec1[0] != S_INTERP:
            rs_tup = self.reason_summary[tuple_to_reason_index(self.to_interp)]
            rs_tup[0] += 1
            rs_tup[1] += dt

    def enter(self, rec):
        if rec[0] >= event_start:
            self.write(*rec)
        else:
            if self.t0 is None:
                self.t0 = rec[2]
                self.stack.append([0, 0, rec[2]])
            self.transition(self.stack[-1], rec)
            self.stack.append(rec)

    def exit(self, rec):
        entry = self.stack.pop()
        prior = self.stack[-1]
        exit = rec
        exit[0] = prior[0]
        self.transition(entry, exit)
        prior[1] = rec[1]
        prior[2] = rec[2]

def read_history(filename):
    h = History()
    f = open(filename)
    while True:
        raw = f.read(8)
        if not raw:
            break
        ull = struct.unpack_from('Q', raw, 0)[0]
        rec = [(ull >> 60) & 0xf,
               (ull >> 55) & 0x1f,
               ull & ~(0x1ff << 55)]
        if rec[0]:
            h.enter(rec)
        else:
            h.exit(rec)
    return h

def summary(hist):
    ep = 0       # estimated progress

    print 'Activity duration summary:'
    print
    cycles = hist.state_summary
    for sn, cyc, xf in zip(states, cycles, speedups):
        if sn == 'exitlast': continue
        ep += xf * cyc
        print '%-12s %12.6f'%(sn, cyc/CPU_SPEED * 1000)
    print '-'*25
    print '%-12s %12.6f'%('Subtotal', sum(cycles[1:])/CPU_SPEED * 1000)
    print '%-12s %12.6f'%('Non-JS', cycles[0]/CPU_SPEED * 1000)
    print '='*25
    print '%-12s %12.6f'%('Total', sum(cycles)/CPU_SPEED * 1000)

    print 'Estimated speedup:  %.2fx'%(ep/sum(cycles[1:]))

    print
    print 'Reasons for transitions to interpret:'
    print '%-12s %12s %8s %12s'%(
        'Reason', 'sum (ms)', 'N', 'mean (us)')
    rl = [ (rc[1], rc[0], reason_index_label(i))
            for i, rc in enumerate(hist.reason_summary) ]
    rl.sort(reverse=1)
    for cyc, n, label in rl:
        if cyc == 0: continue
        print '%-12s %12.3f %8d %12.3f'%(
            label, 
            cyc/CPU_SPEED*1000, 
            n, 
            cyc/CPU_SPEED*1000000/n,
            )

if __name__ == '__main__':
    if len(sys.argv) <= 1:
        print >> sys.stderr, "usage: python binlog.py infile"
        sys.exit(1);

    filename = sys.argv[1]
    history = read_history(filename)

    summary(history)
