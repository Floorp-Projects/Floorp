#!/usr/bin/python3

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Test command generator for the IPDL parser.

# 1. Add the following code to ipc/ipdl/ipdl.py, do a build, and copy
# the output from the build, from INCLUDES to DONE, somewhere:
#
# print "INCLUDES"
# for i in includedirs:
#     print i
# print "FILES"
# for f in files:
#     print f
# print "DONE"

# 2. Adjust leading_text_example as necessary, if the log timestamp
# stuff has changed.

# 3. Run this script on the output from step 1. This should produce a
# command to run cargo with all of the files from step 1. You can run
# it with bash or whatever.

import sys

# Used to decide how many characters to chop off the start.
leading_text_example = " 0:02.39 "

in_include = False
in_files = False

start_trim = len(leading_text_example)

print("cargo run --"),

for line in sys.stdin:
    line = line [start_trim:-1]
    if line.endswith("INCLUDES"):
        in_include = True
        continue
    if line.endswith("FILES"):
        assert in_include
        in_include = False
        in_files = True
        continue
    if line.endswith("DONE"):
        assert in_files
        exit(0)

    if in_include:
        print("-I", line),
    elif in_files:
        print(line),
    else:
        assert False
