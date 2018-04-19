#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script runs a process and prefixes its output with.
# Usage: run-and-prefix.py prefix command arg0 argv1...

from __future__ import absolute_import, print_function

import os
import subprocess
import sys

sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)
sys.stderr = os.fdopen(sys.stderr.fileno(), 'w', 0)

prefix = sys.argv[1]
args = sys.argv[2:]

p = subprocess.Popen(args, bufsize=0,
                     stdout=subprocess.PIPE,
                     stderr=subprocess.STDOUT,
                     stdin=sys.stdin.fileno(),
                     universal_newlines=True)

while True:
    data = p.stdout.readline()

    if data == b'':
        break

    print('%s> %s' % (prefix, data), end=b'')

sys.exit(p.wait())
