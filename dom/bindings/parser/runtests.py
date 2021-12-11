# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import print_function

import os, sys
import glob
import argparse
import traceback
import WebIDL


class TestHarness(object):
    def __init__(self, test, verbose):
        self.test = test
        self.verbose = verbose
        self.printed_intro = False
        self.passed = 0
        self.failures = []

    def start(self):
        if self.verbose:
            self.maybe_print_intro()

    def finish(self):
        if self.verbose or self.printed_intro:
            print("Finished test %s" % self.test)

    def maybe_print_intro(self):
        if not self.printed_intro:
            print("Starting test %s" % self.test)
            self.printed_intro = True

    def test_pass(self, msg):
        self.passed += 1
        if self.verbose:
            print("TEST-PASS | %s" % msg)

    def test_fail(self, msg):
        self.maybe_print_intro()
        self.failures.append(msg)
        print("TEST-UNEXPECTED-FAIL | %s" % msg)

    def ok(self, condition, msg):
        if condition:
            self.test_pass(msg)
        else:
            self.test_fail(msg)

    def check(self, a, b, msg):
        if a == b:
            self.test_pass(msg)
        else:
            self.test_fail(msg + " | Got %s expected %s" % (a, b))


def run_tests(tests, verbose):
    testdir = os.path.join(os.path.dirname(__file__), "tests")
    if not tests:
        tests = glob.iglob(os.path.join(testdir, "*.py"))
    sys.path.append(testdir)

    all_passed = 0
    failed_tests = []

    for test in tests:
        (testpath, ext) = os.path.splitext(os.path.basename(test))
        _test = __import__(testpath, globals(), locals(), ["WebIDLTest"])

        harness = TestHarness(test, verbose)
        harness.start()
        try:
            _test.WebIDLTest.__call__(WebIDL.Parser(), harness)
        except Exception as ex:
            harness.test_fail("Unhandled exception in test %s: %s" % (testpath, ex))
            traceback.print_exc()
        finally:
            harness.finish()
        all_passed += harness.passed
        if harness.failures:
            failed_tests.append((test, harness.failures))

    if verbose or failed_tests:
        print()
        print("Result summary:")
        print("Successful: %d" % all_passed)
        print("Unexpected: %d" % sum(len(failures) for _, failures in failed_tests))
        for test, failures in failed_tests:
            print("%s:" % test)
            for failure in failures:
                print("TEST-UNEXPECTED-FAIL | %s" % failure)
    return 1 if failed_tests else 0


def get_parser():
    usage = """%(prog)s [OPTIONS] [TESTS]
               Where TESTS are relative to the tests directory."""
    parser = argparse.ArgumentParser(usage=usage)
    parser.add_argument(
        "-q",
        "--quiet",
        action="store_false",
        dest="verbose",
        help="Don't print passing tests.",
        default=None,
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        dest="verbose",
        help="Run tests in verbose mode.",
    )
    parser.add_argument("tests", nargs="*", help="Tests to run")
    return parser


if __name__ == "__main__":
    parser = get_parser()
    args = parser.parse_args()
    if args.verbose is None:
        args.verbose = True

    # Make sure the current directory is in the python path so we can cache the
    # result of the webidlyacc.py generation.
    sys.path.append(".")

    sys.exit(run_tests(args.tests, verbose=args.verbose))
