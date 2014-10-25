#!/usr/bin/env python
# graph_latency.py - graph media latency
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# needs matplotlib (sudo aptitude install python-matplotlib)

import matplotlib.pyplot as plt
from matplotlib import rc
import sys
from pprint import pprint
import re


# FIX!  needs to be sum of a single mediastreamtrack and any output overhead for it
# So there is one sum per MST
def compute_sum(data):
    'Compute the sum for each timestamp. This expects the output of parse_data.'
    last_values = {}
    out = ([],[])

    for i in data:
        if i[0] not in last_values.keys():
          last_values[i[0]] = 0
        last_values[i[0]] = float(i[3])
        print last_values
        out[0].append(i[2])
        out[1].append(sum(last_values.values()))
    return out


def clean_data(raw_data):
    '''
    Remove the PR_LOG cruft at the beginning of each line and returns a list of
    tuple.
    '''
    out = []
    for line in raw_data:
        match = re.match(r'(.*)#(.*)', line)
        if match:
	    continue
	else:
            out.append(line.split(": ")[1])
    return out

# returns a list of tuples
def parse_data(raw_lines):
    '''
    Split each line by , and put every bit in a tuple.
    '''
    out = []
    for line in raw_lines:
        out.append(line.split(','))
    return out

if len(sys.argv) == 3:
    name = sys.argv[1]
    channels = int(sys.argv[2])
else:
    print sys.argv[0] + "latency_log"

try:
    f = open(sys.argv[1])
except:
    print "cannot open " + name

raw_lines = f.readlines()
lines = clean_data(raw_lines)
data = parse_data(lines)

final_data = {}

for tupl in data:
    name = tupl[0]
    if tupl[1] != 0:
        name = name+tupl[1]
    if name not in final_data.keys():
        final_data[name] = ([], [])
# sanity-check values
    if float(tupl[3]) < 10*1000:
        final_data[name][0].append(float(tupl[2]))
        final_data[name][1].append(float(tupl[3]))

#overall = compute_sum(data)
#final_data["overall"] = overall

pprint(final_data)

fig = plt.figure()
for i in final_data.keys():
    plt.plot(final_data[i][0], final_data[i][1], label=i)

plt.legend()
plt.suptitle("Latency in ms (y-axis) against time in ms (x-axis).")

size = fig.get_size_inches()
# make it gigantic so we can see things. sometimes, if the graph is too big,
# this errors. reduce the factor so it stays under 2**15.
fig.set_size_inches((size[0]*10, size[1]*2))
name = sys.argv[1][:-4] + ".pdf"
fig.savefig(name)

