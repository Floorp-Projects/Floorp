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

# A little magic to create a single "test suite" from all test_ files
# in this dir.  A single suite makes for prettier output test :)
def suite():
    # Loop over all test_*.py files here
    try:
        me = __file__
    except NameError:
        me = sys.argv[0]
    me = os.path.abspath(me)
    files = os.listdir(os.path.dirname(me))
    suite = unittest.TestSuite()
    # XXX - add the others here!
    #suite.addTest(unittest.FunctionTestCase(import_all))
    for file in files:
        base, ext = os.path.splitext(file)
        if ext=='.py' and os.path.basename(base).startswith("test_"):
            mod = __import__(base)
            if hasattr(mod, "suite"):
                test = mod.suite()
            else:
                test = unittest.defaultTestLoader.loadTestsFromModule(mod)
            suite.addTest(test)
    return suite

class CustomLoader(unittest.TestLoader):
    def loadTestsFromModule(self, module):
        return suite()

try:
    unittest.TestProgram(testLoader=CustomLoader())(argv=sys.argv)
finally:
    from xpcom import _xpcom
    _xpcom.NS_ShutdownXPCOM() # To get leak stats and otherwise ensure life is good.
    ni = _xpcom._GetInterfaceCount()
    ng = _xpcom._GetGatewayCount()
    if ni or ng:
        # The old 'regrtest' that was not based purely on unittest did not
        # do this check at the end - it relied on each module doing it itself.
        # Thus, these leaks are not new, just newly noticed :)  Likely to be
        # something silly like module globals.
        if ni == 6 and ng == 1:
            print "Sadly, there are 6/1 leaks, but these appear normal and benign"
        else:
            print "********* WARNING - Leaving with %d/%d objects alive" % (ni,ng)
    else:
        print "yay! Our leaks have all vanished!"
