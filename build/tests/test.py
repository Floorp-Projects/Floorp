#!/usr/bin/env python

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
# The Original Code is mozilla.org code.
# 
# The Initial Developer of the Original Code is
# Mozilla.org.
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#     Jeff Hammel <jhammel@mozilla.com>     (Original author)
# 
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

"""tests for ManifestDestiny"""

import doctest
import os
import sys
from optparse import OptionParser

def run_tests(raise_on_error=False, report_first=False):

    # add results here
    results = {}

    # doctest arguments
    directory = os.path.dirname(os.path.abspath(__file__))
    extraglobs = {}
    doctest_args = dict(extraglobs=extraglobs,
                        module_relative=False,
                        raise_on_error=raise_on_error)
    if report_first:
        doctest_args['optionflags'] = doctest.REPORT_ONLY_FIRST_FAILURE
                                
    # gather tests
    directory = os.path.dirname(os.path.abspath(__file__))
    tests =  [ test for test in os.listdir(directory)
               if test.endswith('.txt') and test.startswith('test_')]
    os.chdir(directory)

    # run the tests
    for test in tests:
        try:
            results[test] = doctest.testfile(test, **doctest_args)
        except doctest.DocTestFailure, failure:
            raise
        except doctest.UnexpectedException, failure:
            raise failure.exc_info[0], failure.exc_info[1], failure.exc_info[2]
        
    return results
                                

def main(args=sys.argv[1:]):

    # parse command line options
    parser = OptionParser(description=__doc__)
    parser.add_option('--raise', dest='raise_on_error',
                      default=False, action='store_true',
                      help="raise on first error")
    parser.add_option('--report-first', dest='report_first',
                      default=False, action='store_true',
                      help="report the first error only (all tests will still run)")
    parser.add_option('-q', '--quiet', dest='quiet',
                      default=False, action='store_true',
                      help="minimize output")
    options, args = parser.parse_args(args)
    quiet = options.__dict__.pop('quiet')

    # run the tests
    results = run_tests(**options.__dict__)

    # check for failure
    failed = False
    for result in results.values():
        if result[0]: # failure count; http://docs.python.org/library/doctest.html#basic-api
            failed = True
            break
    if failed:
        sys.exit(1) # error
    if not quiet:
        # print results
        print "manifestparser.py: All tests pass!"
        for test in sorted(results.keys()):
            result = results[test]
            print "%s: failed=%s, attempted=%s" % (test, result[0], result[1])
               
if __name__ == '__main__':
    main()
