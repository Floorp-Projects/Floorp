# Library for JSTest tests.
#
# This contains classes that represent an individual test, including
# metadata, and know how to run the tests and determine failures.

import os
import sys
from contextlib import contextmanager

# When run on tbpl, we run each test multiple times with the following
# arguments.
JITFLAGS = {
    'all': [
        [],  # no flags, normal baseline and ion
        ['--ion-eager', '--ion-offthread-compile=off',  # implies --baseline-eager
         '--more-compartments'],
        ['--ion-eager', '--ion-offthread-compile=off',
         '--ion-check-range-analysis', '--ion-extra-checks', '--no-sse3', '--no-threads'],
        ['--baseline-eager'],
        ['--no-blinterp', '--no-baseline', '--no-ion', '--more-compartments'],
        ['--blinterp-eager'],
    ],
    # Like 'all' above but for jstests. This has fewer jit-specific
    # configurations.
    'jstests': [
        [],  # no flags, normal baseline and ion
        ['--ion-eager', '--ion-offthread-compile=off',  # implies --baseline-eager
         '--more-compartments'],
        ['--baseline-eager'],
        ['--no-blinterp', '--no-baseline', '--no-ion', '--more-compartments'],
    ],
    # used by jit_test.py
    'ion': [
        ['--baseline-eager'],
        ['--ion-eager', '--ion-offthread-compile=off', '--more-compartments']
    ],
    # Used for testing WarpBuilder.
    'warp': [
        ['--warp'],
        ['--warp', '--ion-eager', '--ion-offthread-compile=off']
    ],
    # Run reduced variants on debug builds, since they take longer time.
    'debug': [
        [],  # no flags, normal baseline and ion
        ['--ion-eager', '--ion-offthread-compile=off',  # implies --baseline-eager
         '--more-compartments'],
        ['--baseline-eager'],
    ],
    # Cover cases useful for tsan. Note that we test --ion-eager without
    # --ion-offthread-compile=off here, because it helps catch races.
    'tsan': [
        [],
        ['--ion-eager', '--ion-check-range-analysis', '--ion-extra-checks', '--no-sse3'],
        ['--no-blinterp', '--no-baseline', '--no-ion'],
    ],
    'baseline': [
        ['--no-ion'],
    ],
    # Interpreter-only, for tools that cannot handle binary code generation.
    'interp': [
        ['--no-blinterp', '--no-baseline', '--no-asmjs', '--wasm-compiler=none',
         '--no-native-regexp']
    ],
    'none': [
        []  # no flags, normal baseline and ion
    ]
}


def get_jitflags(variant, **kwargs):
    if variant not in JITFLAGS:
        print('Invalid jitflag: "{}"'.format(variant))
        sys.exit(1)
    if variant == 'none' and 'none' in kwargs:
        return kwargs['none']
    return JITFLAGS[variant]


def valid_jitflags():
    return JITFLAGS.keys()


def get_environment_overlay(js_shell):
    """
    Build a dict of additional environment variables that must be set to run
    tests successfully.
    """
    # When updating this also update |buildBrowserEnv| in layout/tools/reftest/runreftest.py.
    env = {
        # Force Pacific time zone to avoid failures in Date tests.
        'TZ': 'PST8PDT',
        # Force date strings to English.
        'LC_ALL': 'en_US.UTF-8',
        # Tell the shell to disable crash dialogs on windows.
        'XRE_NO_WINDOWS_CRASH_DIALOG': '1',
    }
    # Add the binary's directory to the library search path so that we find the
    # nspr and icu we built, instead of the platform supplied ones (or none at
    # all on windows).
    if sys.platform.startswith('linux'):
        env['LD_LIBRARY_PATH'] = os.path.dirname(js_shell)
    elif sys.platform.startswith('darwin'):
        env['DYLD_LIBRARY_PATH'] = os.path.dirname(js_shell)
    elif sys.platform.startswith('win'):
        env['PATH'] = os.path.dirname(js_shell)
    return env


@contextmanager
def change_env(env_overlay):
    # Apply the overlaid environment and record the current state.
    prior_env = {}
    for key, val in env_overlay.items():
        prior_env[key] = os.environ.get(key, None)
        if 'PATH' in key and key in os.environ:
            os.environ[key] = '{}{}{}'.format(val, os.pathsep, os.environ[key])
        else:
            os.environ[key] = val

    try:
        # Execute with the new environment.
        yield

    finally:
        # Restore the prior environment.
        for key, val in prior_env.items():
            if val is not None:
                os.environ[key] = val
            else:
                del os.environ[key]


def get_cpu_count():
    """
    Guess at a reasonable parallelism count to set as the default for the
    current machine and run.
    """
    # Python 2.6+
    try:
        import multiprocessing
        return multiprocessing.cpu_count()
    except (ImportError, NotImplementedError):
        pass

    # POSIX
    try:
        res = int(os.sysconf('SC_NPROCESSORS_ONLN'))
        if res > 0:
            return res
    except (AttributeError, ValueError):
        pass

    # Windows
    try:
        res = int(os.environ['NUMBER_OF_PROCESSORS'])
        if res > 0:
            return res
    except (KeyError, ValueError):
        pass

    return 1


class RefTestCase(object):
    """A test case consisting of a test and an expected result."""

    def __init__(self, root, path, extra_helper_paths=None, wpt=None):
        # str:  path of the tests root dir
        self.root = root
        # str:  path of JS file relative to tests root dir
        self.path = path
        # [str]: Extra options to pass to the shell
        self.options = []
        # [str]: JIT flags to pass to the shell
        self.jitflags = []
        # [str]: flags to never pass to the shell for this test
        self.ignoredflags = []
        # str or None: path to reflect-stringify.js file to test
        # instead of actually running tests
        self.test_reflect_stringify = None
        # bool: True => test is module code
        self.is_module = False
        # bool: True => test is asynchronous and runs additional code after completing the first
        # turn of the event loop.
        self.is_async = False
        # bool: True => run test, False => don't run
        self.enable = True
        # str?: Optional error type
        self.error = None
        # bool: expected result, True => pass
        self.expect = True
        # bool: True => ignore output as 'random'
        self.random = False
        # bool: True => test may run slowly
        self.slow = False

        # The terms parsed to produce the above properties.
        self.terms = None

        # The tag between |...| in the test header.
        self.tag = None

        # Anything occuring after -- in the test header.
        self.comment = None

        self.extra_helper_paths = extra_helper_paths or []
        self.wpt = wpt

    def prefix_command(self):
        """Return the '-f' options needed to run a test with the given path."""
        path = self.path
        prefix = []
        while path != '':
            assert path != '/'
            path = os.path.dirname(path)
            shell_path = os.path.join(self.root, path, 'shell.js')
            if os.path.exists(shell_path):
                prefix.append(shell_path)
                prefix.append('-f')
        prefix.reverse()

        for extra_path in self.extra_helper_paths:
            prefix.append('-f')
            prefix.append(extra_path)

        return prefix

    def abs_path(self):
        return os.path.join(self.root, self.path)

    def get_command(self, prefix):
        cmd = prefix + self.jitflags + self.options + self.prefix_command()
        if self.test_reflect_stringify is not None:
            cmd += [self.test_reflect_stringify, "--check", self.abs_path()]
        elif self.is_module:
            cmd += ["--module", self.abs_path()]
        else:
            cmd += ["-f", self.abs_path()]
        for flag in self.ignoredflags:
            if flag in cmd:
                cmd.remove(flag)
        return cmd

    def __str__(self):
        ans = self.path
        if not self.enable:
            ans += ', skip'
        if self.error is not None:
            ans += ', error=' + self.error
        if not self.expect:
            ans += ', fails'
        if self.random:
            ans += ', random'
        if self.slow:
            ans += ', slow'
        if '-d' in self.options:
            ans += ', debugMode'
        return ans

    @staticmethod
    def build_js_cmd_prefix(js_path, js_args, debugger_prefix):
        parts = []
        if debugger_prefix:
            parts += debugger_prefix
        parts.append(js_path)
        if js_args:
            parts += js_args
        return parts

    def __cmp__(self, other):
        if self.path == other.path:
            return 0
        elif self.path < other.path:
            return -1
        return 1

    def __hash__(self):
        return self.path.__hash__()

    def __repr__(self):
        return "<lib.tests.RefTestCase %s>" % (self.path,)
