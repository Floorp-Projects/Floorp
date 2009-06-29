# Parse the binary log file of activity timings.
#
# If run as a script, print a summary of the log, giving the total
# time in each activity.

import sys
import struct

from config import *
from acts import *

def unpack(raw):
    for i in range(0, len(raw), 8):
        ull = struct.unpack_from('Q', raw, i)[0]
        s = (ull >> 60) & 0xf
        r = (ull >> 55) & 0x1f
        t = ull & ~(0x1ff << 55)
        yield s, r, t

def parse_input(filename):
    f = open(filename)
    raw = f.read()
    return unpack(raw)

class TimeCount(object):
    def __init__(self):
        self.t = 0
        self.t2 = 0
        self.ta = 1e30
        self.tz = -1e30
        self.n = 0

    def add(self, dt):
        self.t += dt
        self.t2 += dt*dt
        self.n += 1
        if dt < self.ta:
            self.ta = dt
        if dt > self.tz:
            self.tz = dt

class StackPoint(object):
    def __init__(self, s, r, t):
        self.s = s
        self.r = r
        self.t = t

class TimePoint(object):
    """Transition from state s0 to s1 at time t."""
    def __init__(self, t, r, s0, s1):
        self.t = t
        self.r = r
        self.s0 = s0
        self.s1 = s1

class History(object):
    def __init__(self):
        self.state_summary = [ 0 ] * len(states)
        self.stack = None
        self.init()

    def init(self):
        pass

    def record(self, t, r, s0, s1):
        pass

    def goto(self, s0, t0, s1, r, t1):
        dt = t1 - t0
        self.state_summary[s0] += dt
        self.record(t1, r, s0, s1)

    def enter(self, s, r, t):
        if self.stack is None:
            self.stack = [ StackPoint(0, 0, t) ]
        sp = self.stack[-1]
        self.goto(sp.s, sp.t, s, r, t)
        self.stack.append(StackPoint(s, r, t))

    def exit(self, r, t):
        sp = self.stack.pop()
        osp = self.stack[-1]
        self.goto(sp.s, sp.t, osp.s, r, t)
        osp.t = t
        osp.r = r

class FullHistory(History):
    def init(self):
        self.timeline = []

    def record(self, t1, r, s0, s1):
        self.timeline.append(TimePoint(t1, r, s0, s1))
        
def collect(ts, full=False):
    if full:
        cls = FullHistory
    else:
        cls = History
    h = cls()

    for s, r, t in ts:
        #if t < 1160909415950669-877953307: continue
        #if t > 1160909415950669: break
        if s:
            h.enter(s, r, t)
        else:
            h.exit(r, t)

    #assert len(h.stack) == 1
    #assert sum(h.state_summary) == ts[-1][2] - ts[0][2]
    return h

reason_cache = {}

def getreason(hist, i):
    ans = None
    while True:
        p0 = hist.timeline[i]
        r = p0.r
        if r:
            ans = reasons[r]
            break

        if not p0.s0:
            ans = 'start'
            break

        name = states[p0.s0]
        if name != 'interpret':
            ans = name
            break

        i -= 1
        r = reason_cache.get(i)
        if r:
            ans = r
            break
    reason_cache[i] = ans
    return ans

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
    xd = {}
    for i in range(len(hist.timeline)-1):
        p0 = hist.timeline[i]
        if states[p0.s1] == 'interpret':
            p1 = hist.timeline[i+1]
            name = getreason(hist, i)
            x = xd.get(name)
            if not x:
                x = xd[name] = TimeCount()
            x.add(p1.t - p0.t)
    print '%-12s %12s %8s %12s %12s %12s'%(
        'Reason', 'sum (ms)', 'N', 'mean (us)', 'min (us)', 'max (us)')
    for name, x in sorted(xd.items(), key=lambda p: -p[1].t):
        if not x.n: continue
        print '%-12s %12.3f %8d %12.3f %12.3f %12.3f'%(
            name, 
            x.t/CPU_SPEED*1000, 
            x.n, 
            x.t/CPU_SPEED*1000000/x.n,
            x.ta/CPU_SPEED*1000000,
            x.tz/CPU_SPEED*1000000
            )
        

if __name__ == '__main__':
    if len(sys.argv) <= 1:
        print >> sys.stderr, "usage: python binlog.py infile"
        sys.exit(1);

    print sys.argv[0]
    print sys.argv[1]

    filename = sys.argv[1]
    summary(collect(parse_input(filename), True))
