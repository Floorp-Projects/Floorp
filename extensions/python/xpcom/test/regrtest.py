# Copyright (c) 2000-2001 ActiveState Tool Corporation.
# See the file LICENSE.txt for licensing information.

# regrtest.py
#
# The Regression Tests for the xpcom package.
import os
import sys

import test.regrtest # The standard Python test suite.

path = os.path.abspath(os.path.split(sys.argv[0])[0])
tests = []
for arg in sys.argv[1:]:
    if arg[0] not in "-/":
        tests.append(arg)
tests = tests or test.regrtest.findtests(path, [])
test.regrtest.main(tests, path)