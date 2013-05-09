# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import mozpack.path
import os
import re

from mozbuild.base import (
    MachCommandBase,
    MozbuildObject,
)

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)


DEBUGGER_HELP = 'Debugger binary to run test in. Program name or path.'


class ReftestRunner(MozbuildObject):
    """Easily run reftests.

    This currently contains just the basics for running reftests. We may want
    to hook up result parsing, etc.
    """

    def _manifest_file(self, suite):
        """Returns the manifest file used for a given test suite."""
        files = {
          'reftest': 'reftest.list',
          'reftest-ipc': 'reftest.list',
          'crashtest': 'crashtests.list',
          'crashtest-ipc': 'crashtests.list',
        }
        assert suite in files
        return files[suite]

    def _find_manifest(self, suite, test_file):
        assert test_file
        path_arg = self._wrap_path_argument(test_file)
        relpath = path_arg.relpath()

        if os.path.isdir(path_arg.srcdir_path()):
            return mozpack.path.join(relpath, self._manifest_file(suite))

        if relpath.endswith('.list'):
            return relpath

        raise Exception('Running a single test is not currently supported')

    def _make_shell_string(self, s):
        return "'%s'" % re.sub("'", r"'\''", s)

    def run_reftest_test(self, test_file=None, filter=None, suite=None,
            debugger=None):
        """Runs a reftest.

        test_file is a path to a test file. It can be a relative path from the
        top source directory, an absolute filename, or a directory containing
        test files.

        filter is a regular expression (in JS syntax, as could be passed to the
        RegExp constructor) to select which reftests to run from the manifest.

        suite is the type of reftest to run. It can be one of ('reftest',
        'crashtest').

        debugger is the program name (in $PATH) or the full path of the
        debugger to run.
        """

        if suite not in ('reftest', 'reftest-ipc', 'crashtest', 'crashtest-ipc'):
            raise Exception('None or unrecognized reftest suite type.')

        env = {}
        extra_args = []

        if test_file:
            path = self._find_manifest(suite, test_file)
            if not os.path.exists(mozpack.path.join(self.topsrcdir, path)):
                raise Exception('No manifest file was found at %s.' % path)
            env[b'TEST_PATH'] = path
        if filter:
            extra_args.extend(['--filter', self._make_shell_string(filter)])

        pass_thru = False

        if debugger:
            extra_args.append('--debugger=%s' % debugger)
            pass_thru = True

        if extra_args:
            args = [os.environ.get(b'EXTRA_TEST_ARGS', '')]
            args.extend(extra_args)
            env[b'EXTRA_TEST_ARGS'] = ' '.join(args)

        # TODO hook up harness via native Python
        return self._run_make(directory='.', target=suite, append_env=env,
            pass_thru=pass_thru, ensure_exit_code=False)


def ReftestCommand(func):
    """Decorator that adds shared command arguments to reftest commands."""

    debugger = CommandArgument('--debugger', metavar='DEBUGGER',
        help=DEBUGGER_HELP)
    func = debugger(func)

    flter = CommandArgument('--filter', metavar='REGEX',
        help='A JS regular expression to match test URLs against, to select '
             'a subset of tests to run.')
    func = flter(func)

    path = CommandArgument('test_file', nargs='?', metavar='MANIFEST',
        help='Reftest manifest file, or a directory in which to select '
             'reftest.list. If omitted, the entire test suite is executed.')
    func = path(func)

    return func


@CommandProvider
class MachCommands(MachCommandBase):
    @Command('reftest', category='testing', description='Run reftests.')
    @ReftestCommand
    def run_reftest(self, test_file, **kwargs):
        return self._run_reftest(test_file, suite='reftest', **kwargs)

    @Command('reftest-ipc', category='testing',
        description='Run IPC reftests.')
    @ReftestCommand
    def run_ipc(self, test_file, **kwargs):
        return self._run_reftest(test_file, suite='reftest-ipc', **kwargs)

    @Command('crashtest', category='testing',
        description='Run crashtests.')
    @ReftestCommand
    def run_crashtest(self, test_file, **kwargs):
        return self._run_reftest(test_file, suite='crashtest', **kwargs)

    @Command('crashtest-ipc', category='testing',
        description='Run IPC crashtests.')
    @ReftestCommand
    def run_crashtest_ipc(self, test_file, **kwargs):
        return self._run_reftest(test_file, suite='crashtest-ipc', **kwargs)

    def _run_reftest(self, test_file=None, filter=None, suite=None,
            debugger=None):
        reftest = self._spawn(ReftestRunner)
        return reftest.run_reftest_test(test_file, filter=filter, suite=suite,
            debugger=debugger)


