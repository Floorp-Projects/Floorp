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
# The Original Code is WebIDL Parser.
#
# The Initial Developer of the Original Code is
# the Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Kyle Huey <me@kylehuey.com>
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

import os, sys
import glob
import optparse
import traceback
import WebIDL

class TestHarness(object):
    def __init__(self, test, verbose):
        self.test = test
        self.verbose = verbose
        self.printed_intro = False

    def start(self):
        if self.verbose:
            self.maybe_print_intro()

    def finish(self):
        if self.verbose or self.printed_intro:
            print "Finished test %s" % self.test

    def maybe_print_intro(self):
        if not self.printed_intro:
            print "Starting test %s" % self.test
            self.printed_intro = True

    def test_pass(self, msg):
        if self.verbose:
            print "TEST-PASS | %s" % msg

    def test_fail(self, msg):
        self.maybe_print_intro()
        print "TEST-UNEXPECTED-FAIL | %s" % msg

    def ok(self, condition, msg):
        if condition:
            self.test_pass(msg)
        else:
            self.test_fail(msg)

    def check(self, a, b, msg):
        if a == b:
            self.test_pass(msg)
        else:
            self.test_fail(msg)
            print "\tGot %s expected %s" % (a, b)

def run_tests(tests, verbose):
    testdir = os.path.join(os.path.dirname(__file__), 'tests')
    if not tests:
        tests = glob.iglob(os.path.join(testdir, "*.py"))
    sys.path.append(testdir)

    for test in tests:
        (testpath, ext) = os.path.splitext(os.path.basename(test))
        _test = __import__(testpath, globals(), locals(), ['WebIDLTest'])

        harness = TestHarness(test, verbose)
        harness.start()
        try:
            _test.WebIDLTest.__call__(WebIDL.Parser(), harness)
        except:
            print "TEST-UNEXPECTED-FAIL | Unhandled exception in test %s" % testpath
            traceback.print_exc()
        finally:
            harness.finish()

if __name__ == '__main__':
    usage = """%prog [OPTIONS] [TESTS]
               Where TESTS are relative to the tests directory."""
    parser = optparse.OptionParser(usage=usage)
    parser.add_option('-q', '--quiet', action='store_false', dest='verbose', default=True,
                      help="Don't print passing tests.")
    options, tests = parser.parse_args()

    run_tests(tests, verbose=options.verbose)
