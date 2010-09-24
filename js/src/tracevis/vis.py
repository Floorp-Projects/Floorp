import os, struct, sys
import Image, ImageDraw, ImageFont

from config import *
from acts import *
from binlog import read_history
from progressbar import ProgressBar, default_widgets
from time import clock

BLACK = (0, 0, 0)
WHITE = (255, 255, 255)
font = ImageFont.load_default()

states = [
    (0, 0, 0),
    (225, 225, 225), #(170, 20, 10),
    (225, 30, 12),
    (140, 0, 255),
    (0, 0, 255),
    (180, 180, 30),
    (0, 160, 30),
]

timeslices_ms = [
    1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000 ]

class Data(object):
    def __init__(self, duration, summary, transitions):
        self.duration = duration
        self.summary = summary
        self.transitions = transitions

def parse_raw(filename):
    hist = read_history(filename)
    return Data(hist.transitions[-1][2] - hist.transitions[0][2],
                hist.state_summary,
                hist.transitions)

def parse_cooked(filename):
    f = open(filename)
    header = f.read(20)
    if header != 'TraceVis-History0001':
        print "Invalid header"
        sys.exit(1)

    duration = struct.unpack_from('Q', f.read(8))[0]
    summary = []
    state_count = struct.unpack_from('I', f.read(4))[0]
    for i in range(state_count):
        summary.append(struct.unpack_from('Q', f.read(8))[0])

    # This method is actually faster than reading it all into a buffer and decoding
    # from there.
    pos = f.tell()
    f.seek(0, os.SEEK_END)
    n = (f.tell() - pos) / 10
    f.seek(pos)

    #pb = ProgressBar('read input', n)
    pb = ProgressBar(maxval=n, widgets=['read-input: ']+default_widgets)
    blip = n / 100

    transitions = []
    for i in range(n):
        if i % blip == 0:
            pb.update(i)
        raw = f.read(10)
        s = struct.unpack_from('B', raw)[0]
        r = struct.unpack_from('B', raw, 1)[0]
        t = struct.unpack_from('Q', raw, 2)[0]
        transitions.append((s, r, t))
    pb.finish()
    
    return Data(duration, summary, transitions)

def draw(data, outfile):
    total, summary, ts = data.duration, data.summary, data.transitions

    W, H, HA, HB = 1600, 256, 64, 64
    HZ = H + 4 + HA + 4 + HB
    im = Image.new('RGBA', (W, HZ + 20))
    d = ImageDraw.Draw(im)

    # Filter
    if 0:
        a, b = 10*2.2e9, 12*2.2e9
        ts = [(s, t-a) for s, t in ts if a <= t <= b ]
    total_ms = total / CPU_SPEED * 1000

    pp = 1.0 * total / (W*H)
    pc_us = 1.0 * total / CPU_SPEED / W * 1e6
    print "Parsed %d items, t=%f, t/pixel=%f"%(len(ts), total/CPU_SPEED, pp/CPU_SPEED*1000)

    d.rectangle((0, H + 4, W, H + 4 + HA), (255, 255, 255))

    eps = []

    # estimated progress of program run 
    ep = 0

    # text progress indicator for graph generation
    # pb = ProgressBar('draw main', W*H)
    pb = ProgressBar(maxval=W*H, widgets=['draw main: ']+default_widgets)
    blip = W*H//50

    # Flush events. We need to save them and draw them after filling the main area
    # because otherwise they will be overwritten. Format is (x, y, r) to plot flush at,
    # where r is reason code.
    events = []

    ti = 0
    for i in range(W*H):
        if i % blip == 0:
            pb.update(i)
        x, y = i/H, i%H

        t0 = pp * i
        t1 = t0 + pp
        color = [ 0, 0, 0 ]
        tx = t0
        while ti < len(ts):
            state, reason, time = ts[ti];
            # States past this limit are actually events rather than state transitions
            if state >= event_start:
                events.append((x, y, reason))
                ti += 1
                continue

            q = (min(time, t1)-tx)/pp
            c = states[state]
            color[0] += q * c[0]
            color[1] += q * c[1]
            color[2] += q * c[2]
            ep += q * speedups[state]
            if time > t1:
                break
            tx = time
            ti += 1
        
        color[0] = int(color[0])
        color[1] = int(color[1])
        color[2] = int(color[2])

        d.point((x, y), fill=tuple(color))

        if i % H == H - 1:
            eps.append(ep)
            ep = 0

    for x, y, r in events:
        d.ellipse((x-5, y-5, x+5, y+5), fill=(255, 222, 222), outline=(160, 10, 10))
        ch = flush_reasons[r]
        d.text((x-2, y-5), ch, fill=BLACK, font=font)

    pb.finish()

    epmax = 2.5*H

    d.line((0, H+4+HA-1.0*H/epmax*HA, W,  H+4+HA-1.0*H/epmax*HA), fill=(192, 192, 192))
    d.line((0, H+4+HA-2.5*H/epmax*HA, W,  H+4+HA-2.5*H/epmax*HA), fill=(192, 192, 192))

    last = None
    for i in range(W):
        n = 0
        sm = 0
        for j in range(max(0, i-3), min(W-1, i+3)):
            sm += eps[j]
            n += 1
        ep = sm / n

        ep_scaled = 1.0 * ep / epmax * HA
        px = (i, H + 4 + HA - int(ep_scaled))
        if last:
            d.line((px, last), fill=(0, 0, 0))
        last = px
        #d.point((i, H + 4 + HA - int(ep_scaled)), fill=(0, 0, 0))

    t = 0
    ppt = 1.0 * W / total
    if True:
        for dt, c in zip(summary, states):
            x1 = int(t * ppt)
            y1 = H + 4 + HA + 5 
            x2 = int((t+dt) * ppt)
            y2 = H + 4 + HA + 4 + HB
            d.rectangle((x1, y1, x2, y2), fill=c)
            t += dt

    # Time lines
    i = 0
    while True:
        divs = total_ms / timeslices_ms[i]
        if divs < 15: break
        if i == len(timeslices_ms) - 1: break
        i += 1

    timeslice = timeslices_ms[i]
    t = timeslice
    c = (128, 128, 128)
    while t < total_ms:
        x = t / total_ms * W
        y = 0
        while y < HZ:
            d.line((x, y, x, y + 8), fill=c)
            y += 12
        text = '%d ms'%t
        w, h = font.getsize(text)
        d.text((x-w/2, HZ), text, fill=BLACK, font=font)
        t += timeslice

    d.text((0, HZ), '[%.1fus]'%pc_us, fill=BLACK)

    im.save(outfile, "PNG")

if __name__ == '__main__':
    from optparse import OptionParser
    parser = OptionParser()
    parser.add_option('-c', action='store_true', dest='cooked', default=False,
                      help='process cooked format input file')
    options, args = parser.parse_args()

    if len(sys.argv) <= 2:
        print >> sys.stderr, "usage: python vis.py infile outfile"
        sys.exit(1);
    filename = sys.argv[1]
    outfile  = sys.argv[2]
    if not outfile.endswith('.png'):
        print >> sys.stderr, "warning: output filename does not end with .png; output is in png format."

    if options.cooked:
        data = parse_cooked(filename)
    else:
        data = parse_raw(filename)

    draw(data, outfile)
