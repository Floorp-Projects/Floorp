# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with the
# License. You may obtain a copy of the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
# the specific language governing rights and limitations under the License.
#
# The Original Code is the Python XPCOM language bindings.
#
# The Initial Developer of the Original Code is ActiveState Tool Corp.
# Portions created by ActiveState Tool Corp. are Copyright (C) 2000, 2001
# ActiveState Tool Corp.  All Rights Reserved.
#
# Contributor(s): Mark Hammond <MarkH@ActiveState.com> (original author)
#

# regrtest.py
#
# The Regression Tests for the xpcom package.
import os
import sys

import unittest
import test.regrtest # The standard Python test suite.

path = os.path.abspath(os.path.split(sys.argv[0])[0])
# This sucks - python now uses "test." - so to worm around this,
# we append our test path to the test packages!
test.__path__.append(path)

tests = []
for arg in sys.argv[1:]:
    if arg[0] not in "-/":
        tests.append(arg)
tests = tests or test.regrtest.findtests(path, [])
try:
    # unittest based tests first - hopefully soon this will be the default!
    if not sys.argv[1:]:
        for t in "test_misc".split():
            m = __import__(t)
            try:
                unittest.main(m)
            except SystemExit:
                pass
    test.regrtest.main(tests, path)
finally:
    from xpcom import _xpcom
    _xpcom.NS_ShutdownXPCOM() # To get leak stats and otherwise ensure life is good.
