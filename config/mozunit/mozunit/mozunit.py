# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import inspect
import io
import os
from six import (
    BytesIO,
    StringIO,
)
import sys
import unittest
from unittest import TextTestRunner as _TestRunner, TestResult as _TestResult

import pytest
import six

here = os.path.abspath(os.path.dirname(__file__))

try:
    # buildconfig doesn't yet support Python 3, so we can use pathlib to
    # resolve the topsrcdir relative to our current location.
    from pathlib import Path

    topsrcdir = Path(here).parents[2]
except ImportError:
    from mozbuild.base import MozbuildObject

    build = MozbuildObject.from_environment(cwd=here)
    topsrcdir = build.topsrcdir

"""Helper to make python unit tests report the way that the Mozilla
unit test infrastructure expects tests to report.

Usage:

import mozunit

if __name__ == '__main__':
    mozunit.main()
"""


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

    def printStatus(self, status, test, message=""):
        line = "{status} | {file} | {klass}.{test}{sep}{message}".format(
            status=status,
            file=inspect.getfile(test.__class__),
            klass=test.__class__.__name__,
            test=test._testMethodName,
            sep=", " if message else "",
            message=message,
        )
        self.stream.writeln(line)

    def addSuccess(self, test):
        _TestResult.addSuccess(self, test)
        self.printStatus("TEST-PASS", test)

    def addSkip(self, test, reason):
        _TestResult.addSkip(self, test, reason)
        self.printStatus("TEST-SKIP", test)

    def addExpectedFailure(self, test, err):
        _TestResult.addExpectedFailure(self, test, err)
        self.printStatus("TEST-KNOWN-FAIL", test)

    def addUnexpectedSuccess(self, test):
        _TestResult.addUnexpectedSuccess(self, test)
        self.printStatus("TEST-UNEXPECTED-PASS", test)

    def addError(self, test, err):
        _TestResult.addError(self, test, err)
        self.printFail(test, err)
        self.stream.writeln("ERROR: {0}".format(self.getDescription(test)))
        self.stream.writeln(self.errors[-1][1])

    def addFailure(self, test, err):
        _TestResult.addFailure(self, test, err)
        self.printFail(test, err)
        self.stream.writeln("FAIL: {0}".format(self.getDescription(test)))
        self.stream.writeln(self.failures[-1][1])

    def printFail(self, test, err):
        exctype, value, tb = err
        message = value or "NO MESSAGE"
        if hasattr(value, "message"):
            message = value.message.splitlines()[0]
        # Skip test runner traceback levels
        while tb and self._is_relevant_tb_level(tb):
            tb = tb.tb_next
        if tb:
            _, ln, _ = inspect.getframeinfo(tb)[:3]
            message = "line {0}: {1}".format(ln, message)
        self.printStatus("TEST-UNEXPECTED-FAIL", test, message)


class MozTestRunner(_TestRunner):
    def _makeResult(self):
        return _MozTestResult(self.stream, self.descriptions)

    def run(self, test):
        result = self._makeResult()
        test(result)
        return result


def _mocked_file(cls):
    """Create a mocked file class that inherits from the given class."""

    class MockedFile(cls):
        def __init__(self, context, filename, content):
            self.context = context
            self.name = filename
            cls.__init__(self, content)

        def close(self):
            self.context.files[self.name] = self.getvalue()
            cls.close(self)

        def __enter__(self):
            return self

        def __exit__(self, type, value, traceback):
            self.close()

    return MockedFile


MockedStringFile = _mocked_file(StringIO)
MockedBytesFile = _mocked_file(BytesIO)


def normcase(path):
    """
    Normalize the case of `path`.

    Don't use `os.path.normcase` because that also normalizes forward slashes
    to backslashes on Windows.
    """
    if sys.platform.startswith("win"):
        return path.lower()
    return path


class _MockBaseOpen(object):
    """Callable that acts like the open() function; see MockedOpen for more
    info.
    """

    def __init__(self, open, files):
        self.open = open
        self.files = files

    def __call__(
        self, name, mode="r", buffering=None, encoding=None, newline=None, errors=None
    ):
        # open() can be called with an integer "name" (i.e. a file descriptor).
        # We don't generally do this in our codebase, but internal Python
        # libraries sometimes do and we want to handle that cleanly.
        if isinstance(name, int):
            return self.open(
                name,
                mode=mode,
                buffering=buffering,
                encoding=encoding,
                newline=newline,
                errors=errors,
            )
        # buffering is ignored.
        absname = normcase(os.path.abspath(name))
        if "w" in mode:
            file = self._mocked_file(absname, mode)
        elif absname in self.files:
            content = self.files[absname]
            if content is None:
                raise IOError(2, "No such file or directory")
            file = self._mocked_file(absname, mode, content)
        elif "a" in mode:
            read_mode = "rb" if "b" in mode else "r"
            file = self._mocked_file(absname, mode, self.open(name, read_mode).read())
        else:
            file = self.open(name, mode)
        if "a" in mode:
            file.seek(0, os.SEEK_END)
        return file

    def _mocked_file(self, name, mode, content=None):
        raise NotImplementedError("subclass must implement")


class _MockPy2Open(_MockBaseOpen):
    def _mocked_file(self, name, mode, content=None):
        content = six.ensure_binary(content or b"")
        return MockedBytesFile(self, name, content)


class _MockOpen(_MockBaseOpen):
    def _mocked_file(self, name, mode, content=None):
        if "b" in mode:
            content = six.ensure_binary(content or b"")
            return MockedBytesFile(self, name, content)
        else:
            content = six.ensure_text(content or u"")
            return MockedStringFile(self, name, content)


class MockedOpen(object):
    """
    Context manager diverting the open builtin such that opening files
    can open "virtual" file instances given when creating a MockedOpen.

    with MockedOpen({'foo': 'foo', 'bar': 'bar'}):
        f = open('foo', 'r')

    will thus open the virtual file instance for the file 'foo' to f.

    If the content of a file is given as None, then that file will be
    represented as not existing (even if it does, actually, exist).

    MockedOpen also masks writes, so that creating or replacing files
    doesn't touch the file system, while subsequently opening the file
    will return the recorded content.

    with MockedOpen():
        f = open('foo', 'w')
        f.write('foo')
    self.assertRaises(Exception,f.open('foo', 'r'))
    """

    def __init__(self, files={}):
        self.files = {}
        for name, content in files.items():
            self.files[normcase(os.path.abspath(name))] = content

    def __enter__(self):
        import six.moves.builtins

        self.open = six.moves.builtins.open
        self.io_open = io.open
        self._orig_path_exists = os.path.exists
        self._orig_path_isdir = os.path.isdir
        self._orig_path_isfile = os.path.isfile
        builtin_cls = _MockPy2Open if six.PY2 else _MockOpen
        six.moves.builtins.open = builtin_cls(self.open, self.files)
        io.open = _MockOpen(self.io_open, self.files)
        os.path.exists = self._wrapped_exists
        os.path.isdir = self._wrapped_isdir
        os.path.isfile = self._wrapped_isfile

    def __exit__(self, type, value, traceback):
        import six.moves.builtins

        six.moves.builtins.open = self.open
        io.open = self.io_open
        os.path.exists = self._orig_path_exists
        os.path.isdir = self._orig_path_isdir
        os.path.isfile = self._orig_path_isfile

    def _wrapped_exists(self, p):
        return self._wrapped_isfile(p) or self._wrapped_isdir(p)

    def _wrapped_isfile(self, p):
        p = normcase(p)
        if p in self.files:
            return self.files[p] is not None

        abspath = normcase(os.path.abspath(p))
        if abspath in self.files:
            return self.files[abspath] is not None

        return self._orig_path_isfile(p)

    def _wrapped_isdir(self, p):
        p = normcase(p)
        p = p if p.endswith(("/", "\\")) else p + os.sep
        if any(f.startswith(p) for f in self.files):
            return True

        abspath = normcase(os.path.abspath(p) + os.sep)
        if any(f.startswith(abspath) for f in self.files):
            return True

        return self._orig_path_isdir(p)


def main(*args, **kwargs):
    runwith = kwargs.pop("runwith", "pytest")

    if runwith == "unittest":
        unittest.main(testRunner=MozTestRunner(), *args, **kwargs)
    else:
        args = list(args)
        if os.environ.get("MACH_STDOUT_ISATTY") and not any(
            a.startswith("--color") for a in args
        ):
            args.append("--color=yes")

        module = __import__("__main__")
        args.extend(
            [
                "--rootdir",
                topsrcdir,
                "-c",
                os.path.join(here, "pytest.ini"),
                "-vv",
                "-p",
                "mozlog.pytest_mozlog.plugin",
                "-p",
                "mozunit.pytest_plugin",
                "-p",
                "no:cacheprovider",
                "-rsx",  # show reasons for skip / xfail
                module.__file__,
            ]
        )
        sys.exit(pytest.main(args))
