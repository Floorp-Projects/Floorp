#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script runs a process and prefixes its output with.
# Usage: run-and-prefix.py prefix command arg0 argv1...

import os
import subprocess
import sys

sys.stdout = os.fdopen(sys.stdout.fileno(), "wb", 0)
sys.stderr = os.fdopen(sys.stderr.fileno(), "wb", 0)

prefix = sys.argv[1].encode("utf-8")
args = sys.argv[2:]

p = subprocess.Popen(
    args,
    bufsize=0,
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    stdin=sys.stdin.fileno(),
    close_fds=False,
)

while True:
    data = p.stdout.readline()

    if data == b"":
        break

    sys.stdout.write(b"%s> %s" % (prefix, data))

sys.exit(p.wait())
