# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Take the *.framedata files from graph-frameclasses.js and combine them
into a single graphviz file.

stdin: a list of .framedata file names (e.g. from xargs)
stdout: a graphviz file

e.g. `find <objdir> -name "*.framedata" | python aggregate-frameclasses.py |
  dot -Tpng -o frameclasses-graph.png -`
"""

import sys

classdict = {}

for line in sys.stdin:
    file = line.strip()
    fd = open(file)

    output = None
    for line in fd:
        if line.startswith('CLASS-DEF: '):
            cname = line[11:-1]
            if cname not in classdict:
                output = classdict[cname] = []
            else:
                output = None
        elif output is not None:
            output.append(line)

sys.stdout.write('digraph g {\n')

for olist in classdict.itervalues():
    for line in olist:
        sys.stdout.write(line)

sys.stdout.write('}\n')
