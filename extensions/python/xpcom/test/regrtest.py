# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Python XPCOM language bindings.
#
# The Initial Developer of the Original Code is
# ActiveState Tool Corp.
# Portions created by the Initial Developer are Copyright (C) 2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Mark Hammond <mhammond@skippinet.com.au> (original author)
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

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
