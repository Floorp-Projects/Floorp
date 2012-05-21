# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from unittest import TextTestRunner as _TestRunner, TestResult as _TestResult
import inspect

'''Helper to make python unit tests report the way that the Mozilla
unit test infrastructure expects tests to report.

Usage:

import unittest
from mozunit import MozTestRunner

if __name__ == '__main__':
    unittest.main(testRunner=MozTestRunner())
'''

class _MozTestResult(_TestResult):
    def __init__(self, stream, descriptions):
        _TestResult.__init__(self)
        self.stream = stream
        self.descriptions = descriptions

    def getDescription(self, test):
        if self.descriptions:
            return test.shortDescription() or str(test)
        else:
            return str(test)

    def addSuccess(self, test):
        _TestResult.addSuccess(self, test)
        filename = inspect.getfile(test.__class__)
        testname = test._testMethodName
        self.stream.writeln("TEST-PASS | %s | %s" % (filename, testname))

    def addError(self, test, err):
        _TestResult.addError(self, test, err)
        self.printFail(test, err)

    def addFailure(self, test, err):
        _TestResult.addFailure(self, test, err)
        self.printFail(test,err)

    def printFail(self, test, err):
        exctype, value, tb = err
        # Skip test runner traceback levels
        while tb and self._is_relevant_tb_level(tb):
            tb = tb.tb_next
        if not tb:
            self.stream.writeln("TEST-UNEXPECTED-FAIL | NO TRACEBACK |")
        _f, _ln, _t = inspect.getframeinfo(tb)[:3]
        self.stream.writeln("TEST-UNEXPECTED-FAIL | %s | line %d, %s: %s" % 
                            (_f, _ln, _t, value.message))

    def printErrorList(self):
        for test, err in self.errors:
            self.stream.writeln("ERROR: %s" % self.getDescription(test))
            self.stream.writeln("%s" % err)


class MozTestRunner(_TestRunner):
    def _makeResult(self):
        return _MozTestResult(self.stream, self.descriptions)
    def run(self, test):
        result = self._makeResult()
        test(result)
        result.printErrorList()
        return result
