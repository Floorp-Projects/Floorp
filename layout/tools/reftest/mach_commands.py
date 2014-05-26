# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import mozpack.path
import os
import re
import sys
import warnings
import which

from mozbuild.base import (
    MachCommandBase,
    MachCommandConditions as conditions,
    MozbuildObject,
)

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)


DEBUGGER_HELP = 'Debugger binary to run test in. Program name or path.'

ADB_NOT_FOUND = '''
The %s command requires the adb binary to be on your path.

If you have a B2G build, this can be found in
'%s/out/host/<platform>/bin'.
'''.lstrip()

GAIA_PROFILE_NOT_FOUND = '''
The %s command requires a non-debug gaia profile. Either pass in --profile,
or set the GAIA_PROFILE environment variable.

If you do not have a non-debug gaia profile, you can build one:
    $ git clone https://github.com/mozilla-b2g/gaia
    $ cd gaia
    $ make

The profile should be generated in a directory called 'profile'.
'''.lstrip()

GAIA_PROFILE_IS_DEBUG = '''
The %s command requires a non-debug gaia profile. The specified profile,
%s, is a debug profile.

If you do not have a non-debug gaia profile, you can build one:
    $ git clone https://github.com/mozilla-b2g/gaia
    $ cd gaia
    $ make

The profile should be generated in a directory called 'profile'.
'''.lstrip()

MARIONETTE_DISABLED = '''
The %s command requires a marionette enabled build.

Add 'ENABLE_MARIONETTE=1' to your mozconfig file and re-build the application.
Your currently active mozconfig is %s.
'''.lstrip()

class ReftestRunner(MozbuildObject):
    """Easily run reftests.

    This currently contains just the basics for running reftests. We may want
    to hook up result parsing, etc.
    """
    def __init__(self, *args, **kwargs):
        MozbuildObject.__init__(self, *args, **kwargs)

        # TODO Bug 794506 remove once mach integrates with virtualenv.
        build_path = os.path.join(self.topobjdir, 'build')
        if build_path not in sys.path:
            sys.path.append(build_path)

        self.tests_dir = os.path.join(self.topobjdir, '_tests')
        self.reftest_dir = os.path.join(self.tests_dir, 'reftest')

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

    def run_b2g_test(self, b2g_home=None, xre_path=None, test_file=None,
                     suite=None, **kwargs):
        """Runs a b2g reftest.

        test_file is a path to a test file. It can be a relative path from the
        top source directory, an absolute filename, or a directory containing
        test files.

        suite is the type of reftest to run. It can be one of ('reftest',
        'crashtest').
        """
        if suite not in ('reftest', 'crashtest'):
            raise Exception('None or unrecognized reftest suite type.')

        # Find the manifest file
        if not test_file:
            if suite == 'reftest':
                test_file = mozpack.path.join('layout', 'reftests')
            elif suite == 'crashtest':
                test_file = mozpack.path.join('testing', 'crashtest')

        if not os.path.exists(os.path.join(self.topsrcdir, test_file)):
            test_file = mozpack.path.relpath(os.path.abspath(test_file),
                                             self.topsrcdir)

        manifest = self._find_manifest(suite, test_file)
        if not os.path.exists(mozpack.path.join(self.topsrcdir, manifest)):
            raise Exception('No manifest file was found at %s.' % manifest)

        # Need to chdir to reftest_dir otherwise imports fail below.
        os.chdir(self.reftest_dir)

        # The imp module can spew warnings if the modules below have
        # already been imported, ignore them.
        with warnings.catch_warnings():
            warnings.simplefilter('ignore')

            import imp
            path = os.path.join(self.reftest_dir, 'runreftestb2g.py')
            with open(path, 'r') as fh:
                imp.load_module('reftest', fh, path, ('.py', 'r', imp.PY_SOURCE))
            import reftest

        # Set up the reftest options.
        parser = reftest.B2GOptions()
        options, args = parser.parse_args([])

        # Tests need to be served from a subdirectory of the server. Symlink
        # topsrcdir here to get around this.
        tests = os.path.join(self.reftest_dir, 'tests')
        if not os.path.isdir(tests):
            os.symlink(self.topsrcdir, tests)
        args.insert(0, os.path.join('tests', manifest))

        for k, v in kwargs.iteritems():
            setattr(options, k, v)

        if conditions.is_b2g_desktop(self):
            if self.substs.get('ENABLE_MARIONETTE') != '1':
                print(MARIONETTE_DISABLED % ('mochitest-b2g-desktop',
                                             self.mozconfig['path']))
                return 1

            options.profile = options.profile or os.environ.get('GAIA_PROFILE')
            if not options.profile:
                print(GAIA_PROFILE_NOT_FOUND % 'reftest-b2g-desktop')
                return 1

            if os.path.isfile(os.path.join(options.profile, 'extensions', \
                    'httpd@gaiamobile.org')):
                print(GAIA_PROFILE_IS_DEBUG % ('mochitest-b2g-desktop',
                                               options.profile))
                return 1

            options.desktop = True
            options.app = self.get_binary_path()
            if options.oop:
                options.browser_arg = '-oop'
            if not options.app.endswith('-bin'):
                options.app = '%s-bin' % options.app
            if not os.path.isfile(options.app):
                options.app = options.app[:-len('-bin')]

            return reftest.run_desktop_reftests(parser, options, args)


        try:
            which.which('adb')
        except which.WhichError:
            # TODO Find adb automatically if it isn't on the path
            raise Exception(ADB_NOT_FOUND % ('%s-remote' % suite, b2g_home))

        options.b2gPath = b2g_home
        options.logcat_dir = self.reftest_dir
        options.httpdPath = os.path.join(self.topsrcdir, 'netwerk', 'test', 'httpserver')
        options.xrePath = xre_path
        options.ignoreWindowSize = True

        # Don't enable oop for crashtest until they run oop in automation
        if suite == 'reftest':
            options.oop = True

        return reftest.run_remote_reftests(parser, options, args)

    def run_desktop_test(self, test_file=None, filter=None, suite=None,
            debugger=None, parallel=False, shuffle=False,
            e10s=False, this_chunk=None, total_chunks=None):
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

        parallel indicates whether tests should be run in parallel or not.

        shuffle indicates whether to run tests in random order.
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

        if parallel:
            extra_args.append('--run-tests-in-parallel')

        if shuffle:
            extra_args.append('--shuffle')

        if e10s:
            extra_args.append('--e10s')

        if this_chunk:
            extra_args.append('--this-chunk=%s' % this_chunk)

        if total_chunks:
            extra_args.append('--total-chunks=%s' % total_chunks)

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

    parallel = CommandArgument('--parallel', action='store_true',
        help='Run tests in parallel.')
    func = parallel(func)

    shuffle = CommandArgument('--shuffle', action='store_true',
        help='Run tests in random order.')
    func = shuffle(func)

    e10s = CommandArgument('--e10s', action='store_true',
        help='Use content processes.')
    func = e10s(func)

    totalChunks = CommandArgument('--total-chunks',
        help = 'How many chunks to split the tests up into.')
    func = totalChunks(func)

    thisChunk = CommandArgument('--this-chunk',
        help = 'Which chunk to run between 1 and --total-chunks.')
    func = thisChunk(func)

    return func

def B2GCommand(func):
    """Decorator that adds shared command arguments to b2g mochitest commands."""

    busybox = CommandArgument('--busybox', default=None,
        help='Path to busybox binary to install on device')
    func = busybox(func)

    logcatdir = CommandArgument('--logcat-dir', default=None,
        help='directory to store logcat dump files')
    func = logcatdir(func)

    sdcard = CommandArgument('--sdcard', default="10MB",
        help='Define size of sdcard: 1MB, 50MB...etc')
    func = sdcard(func)

    emulator_res = CommandArgument('--emulator-res', default='800x1000',
        help='Emulator resolution of the format \'<width>x<height>\'')
    func = emulator_res(func)

    marionette = CommandArgument('--marionette', default=None,
        help='host:port to use when connecting to Marionette')
    func = marionette(func)

    totalChunks = CommandArgument('--total-chunks', dest='totalChunks',
        help = 'How many chunks to split the tests up into.')
    func = totalChunks(func)

    thisChunk = CommandArgument('--this-chunk', dest='thisChunk',
        help = 'Which chunk to run between 1 and --total-chunks.')
    func = thisChunk(func)

    oop = CommandArgument('--enable-oop', action='store_true', dest='oop',
        help = 'Run tests in out-of-process mode.')
    func = oop(func)

    path = CommandArgument('test_file', default=None, nargs='?',
        metavar='TEST',
        help='Test to run. Can be specified as a single file, a ' \
            'directory, or omitted. If omitted, the entire test suite is ' \
            'executed.')
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

    def _run_reftest(self, test_file=None, suite=None, **kwargs):
        reftest = self._spawn(ReftestRunner)
        return reftest.run_desktop_test(test_file, suite=suite, **kwargs)


# TODO For now b2g commands will only work with the emulator,
# they should be modified to work with all devices.
def is_emulator(cls):
    """Emulator needs to be configured."""
    return cls.device_name.startswith('emulator')


@CommandProvider
class B2GCommands(MachCommandBase):
    def __init__(self, context):
        MachCommandBase.__init__(self, context)

        for attr in ('b2g_home', 'xre_path', 'device_name'):
            setattr(self, attr, getattr(context, attr, None))

    @Command('reftest-remote', category='testing',
        description='Run a remote reftest.',
        conditions=[conditions.is_b2g, is_emulator])
    @B2GCommand
    def run_reftest_remote(self, test_file, **kwargs):
        return self._run_reftest(test_file, suite='reftest', **kwargs)

    @Command('reftest-b2g-desktop', category='testing',
        description='Run a b2g desktop reftest.',
        conditions=[conditions.is_b2g_desktop])
    @B2GCommand
    def run_reftest_b2g_desktop(self, test_file, **kwargs):
        return self._run_reftest(test_file, suite='reftest', **kwargs)

    @Command('crashtest-remote', category='testing',
        description='Run a remote crashtest.',
        conditions=[conditions.is_b2g, is_emulator])
    @B2GCommand
    def run_crashtest_remote(self, test_file, **kwargs):
        return self._run_reftest(test_file, suite='crashtest', **kwargs)

    def _run_reftest(self, test_file=None, suite=None, **kwargs):
        if self.device_name:
            if self.device_name.startswith('emulator'):
                emulator = 'arm'
                if 'x86' in self.device_name:
                    emulator = 'x86'
                kwargs['emulator'] = emulator

        reftest = self._spawn(ReftestRunner)
        return reftest.run_b2g_test(self.b2g_home, self.xre_path,
            test_file, suite=suite, **kwargs)
