import sys
import Image, ImageDraw, ImageFont

from config import *
from acts import *
from binlog import parse_input, collect

BLACK = (0, 0, 0)
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

def parse(filename):
    return collect(parse_input(filename), True)

def run(filename, outfile):
    W, H, HA, HB = 1600, 256, 64, 64
    HZ = H + 4 + HA + 4 + HB
    im = Image.new('RGBA', (W, HZ + 20))
    d = ImageDraw.Draw(im)

    hist = parse(filename)

    t0 = hist.timeline[0].t
    ts = []
    for x in hist.timeline:
        ts.append((x.s0, x.t-t0))

    # Filter
    if 0:
        a, b = 10*2.2e9, 12*2.2e9
        ts = [(s, t-a) for s, t in ts if a <= t <= b ]
    total = ts[-1][1] - ts[0][1]
    total_ms = total / CPU_SPEED * 1000

    pp = 1.0 * total / (W*H)
    pc_us = 1.0 * total / CPU_SPEED / W * 1e6
    print "Parsed %d items, t=%f, t/pixel=%f, t0=%d"%(len(ts), total/CPU_SPEED, pp/CPU_SPEED*1000, hist.timeline[0].t)

    d.rectangle((0, H + 4, W, H + 4 + HA), (255, 255, 255))

    eps = []

    # estimated progress
    ep = 0

    ti = 0
    for i in range(W*H):
        t0 = pp * i
        t1 = t0 + pp
        color = [ 0, 0, 0 ]
        tx = t0
        while ti < len(ts):
            q = (min(ts[ti][1], t1)-tx)/pp
            c = states[ts[ti][0]]
            color[0] += q * c[0]
            color[1] += q * c[1]
            color[2] += q * c[2]
            ep += q * speedups[ts[ti][0]]
            if ts[ti][1] > t1:
                break
            tx = ts[ti][1]
            ti += 1
        
        color[0] = int(color[0])
        color[1] = int(color[1])
        color[2] = int(color[2])

        d.point((i/H, i%H), fill=tuple(color))

        if i % H == H - 1:
            eps.append(ep)
            ep = 0

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
    for dt, c in zip(hist.state_summary, states):
        x1 = int(t * ppt)
        y1 = H + 4 + HA + 5 
        x2 = int((t+dt) * ppt)
        y2 = H + 4 + HA + 4 + HB
        d.rectangle((x1, y1, x2, y2), fill=c)
        t += dt

    # Time lines
    i = 0
    while i < len(timeslices_ms):
        divs = total_ms / timeslices_ms[i]
        if divs < 15: break
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
    if len(sys.argv) <= 2:
        print >> sys.stderr, "usage: python vis.py infile outfile"
        sys.exit(1);

    filename = sys.argv[1]
    outfile  = sys.argv[2]
    if not outfile.endswith('.png'):
        print >> sys.stderr, "warning: output filename does not end with .png; output is in png format."

    run(filename, outfile)

