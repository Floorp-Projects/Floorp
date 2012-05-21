#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import sys
import pymake.parser

for f in sys.argv[1:]:
    print "Parsing %s" % f
    fd = open(f, 'rU')
    s = fd.read()
    fd.close()
    stmts = pymake.parser.parsestring(s, f)
    print stmts
