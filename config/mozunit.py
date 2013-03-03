# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from unittest import TextTestRunner as _TestRunner, TestResult as _TestResult
import unittest
import inspect
from StringIO import StringIO
import os

'''Helper to make python unit tests report the way that the Mozilla
unit test infrastructure expects tests to report.

Usage:

import unittest
import mozunit

if __name__ == '__main__':
    mozunit.main()
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
        self.stream.writeln("TEST-PASS | {0} | {1}".format(filename, testname))

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
        self.stream.writeln("TEST-UNEXPECTED-FAIL | {0} | line {1}, {2}: {3}" 
                            .format(_f, _ln, _t, value.message))

    def printErrorList(self):
        for test, err in self.errors:
            self.stream.writeln("ERROR: {0}".format(self.getDescription(test)))
            self.stream.writeln("{0}".format(err))


class MozTestRunner(_TestRunner):
    def _makeResult(self):
        return _MozTestResult(self.stream, self.descriptions)
    def run(self, test):
        result = self._makeResult()
        test(result)
        result.printErrorList()
        return result

class MockedFile(StringIO):
    def __init__(self, context, filename, content = ''):
        self.context = context
        self.name = filename
        StringIO.__init__(self, content)

    def close(self):
        self.context.files[self.name] = self.getvalue()
        StringIO.close(self)

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        self.close()

class MockedOpen(object):
    '''
    Context manager diverting the open builtin such that opening files
    can open "virtual" file instances given when creating a MockedOpen.

    with MockedOpen({'foo': 'foo', 'bar': 'bar'}):
        f = open('foo', 'r')

    will thus open the virtual file instance for the file 'foo' to f.

    MockedOpen also masks writes, so that creating or replacing files
    doesn't touch the file system, while subsequently opening the file
    will return the recorded content.

    with MockedOpen():
        f = open('foo', 'w')
        f.write('foo')
    self.assertRaises(Exception,f.open('foo', 'r'))
    '''
    def __init__(self, files = {}):
        self.files = {}
        for name, content in files.iteritems():
            self.files[os.path.abspath(name)] = content

    def __call__(self, name, mode = 'r'):
        absname = os.path.abspath(name)
        if 'w' in mode:
            file = MockedFile(self, absname)
        elif absname in self.files:
            file = MockedFile(self, absname, self.files[absname])
        elif 'a' in mode:
            file = MockedFile(self, absname, self.open(name, 'r').read())
        else:
            file = self.open(name, mode)
        if 'a' in mode:
            file.seek(0, os.SEEK_END)
        return file

    def __enter__(self):
        import __builtin__
        self.open = __builtin__.open
        __builtin__.open = self

    def __exit__(self, type, value, traceback):
        import __builtin__
        __builtin__.open = self.open

def main(*args):
    unittest.main(testRunner=MozTestRunner(),*args)
