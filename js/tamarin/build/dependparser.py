#!/usr/bin/env python

"""Pipes stdin | stdout and extracts a list of all header dependencies to
a file."""

import re
import sys
from sets import Set

_lineExp = re.compile("# \d+ \"([^\"<>]+)\"");

deps = Set()

for line in sys.stdin:
    sys.stdout.write(line)
    m = _lineExp.match(line)
    if m:
	deps.add(m.group(1))

if len(sys.argv) != 2:
    raise Exception("Unexpected command line argument.")

outfile = sys.argv[1]
ostream = open(outfile, "w")
ostream.write("\n".join(deps))
ostream.close()
