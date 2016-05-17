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

    def printStatus(self, status, test, message=''):
        line = "{status} | {file} | {klass}.{test}{sep}{message}".format(
            status=status,
            file=inspect.getfile(test.__class__),
            klass=test.__class__.__name__,
            test=test._testMethodName,
            sep=', ' if message else '',
            message=message,
        )
        self.stream.writeln(line)

    def addSuccess(self, test):
        _TestResult.addSuccess(self, test)
        self.printStatus('TEST-PASS', test)

    def addSkip(self, test, reason):
        _TestResult.addSkip(self, test, reason)
        self.printStatus('TEST-SKIP', test)

    def addExpectedFailure(self, test, err):
        _TestResult.addExpectedFailure(self, test, err)
        self.printStatus('TEST-KNOWN-FAIL', test)

    def addUnexpectedSuccess(self, test):
        _TestResult.addUnexpectedSuccess(self, test)
        self.printStatus('TEST-UNEXPECTED-PASS', test)

    def addError(self, test, err):
        _TestResult.addError(self, test, err)
        self.printFail(test, err)
        self.stream.writeln("ERROR: {0}".format(self.getDescription(test)))
        self.stream.writeln(self.errors[-1][1])

    def addFailure(self, test, err):
        _TestResult.addFailure(self, test, err)
        self.printFail(test,err)
        self.stream.writeln("FAIL: {0}".format(self.getDescription(test)))
        self.stream.writeln(self.failures[-1][1])

    def printFail(self, test, err):
        exctype, value, tb = err
        # Skip test runner traceback levels
        while tb and self._is_relevant_tb_level(tb):
            tb = tb.tb_next
        if not tb:
            self.stream.writeln("TEST-UNEXPECTED-FAIL | NO TRACEBACK |")
        _f, _ln, _t = inspect.getframeinfo(tb)[:3]
        self.printStatus('TEST-UNEXPECTED-FAIL', test,
                         'line {0}: {1}'.format(_ln, value.message))


class MozTestRunner(_TestRunner):
    def _makeResult(self):
        return _MozTestResult(self.stream, self.descriptions)
    def run(self, test):
        result = self._makeResult()
        test(result)
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
        self._orig_path_exists = os.path.exists
        __builtin__.open = self
        os.path.exists = self._wrapped_exists

    def __exit__(self, type, value, traceback):
        import __builtin__
        __builtin__.open = self.open
        os.path.exists = self._orig_path_exists

    def _wrapped_exists(self, p):
        if p in self.files:
            return True

        abspath = os.path.abspath(p)
        if abspath in self.files:
            return True

        return self._orig_path_exists(p)

def main(*args, **kwargs):
    unittest.main(testRunner=MozTestRunner(), *args, **kwargs)
