# Library for JSTest tests.
#
# This contains classes that represent an individual test, including
# metadata, and know how to run the tests and determine failures.

import datetime, os, sys, time
from contextlib import contextmanager
from subprocess import Popen, PIPE
from threading import Thread

from results import TestOutput

# When run on tbpl, we run each test multiple times with the following
# arguments.
JITFLAGS = {
    'all': [
        [], # no flags, normal baseline and ion
        ['--ion-eager', '--ion-offthread-compile=off'], # implies --baseline-eager
        ['--ion-eager', '--ion-offthread-compile=off', '--non-writable-jitcode',
         '--ion-check-range-analysis', '--ion-extra-checks', '--no-sse3', '--no-threads'],
        ['--baseline-eager'],
        ['--baseline-eager', '--no-fpu'],
        ['--no-baseline', '--no-ion'],
    ],
    # used by jit_test.py
    'ion': [
        ['--baseline-eager'],
        ['--ion-eager', '--ion-offthread-compile=off']
    ],
    # Run reduced variants on debug builds, since they take longer time.
    'debug': [
        [], # no flags, normal baseline and ion
        ['--ion-eager', '--ion-offthread-compile=off'], # implies --baseline-eager
        ['--baseline-eager'],
    ],
    'none': [
        [] # no flags, normal baseline and ion
    ]
}

def get_jitflags(variant, **kwargs):
    if variant not in JITFLAGS:
        print('Invalid jitflag: "{}"'.format(variant))
        sys.exit(1)
    if variant == 'none' and 'none' in kwargs:
        return kwargs['none']
    return JITFLAGS[variant]


def get_environment_overlay(js_shell):
    """
    Build a dict of additional environment variables that must be set to run
    tests successfully.
    """
    env = {
        # Force Pacific time zone to avoid failures in Date tests.
        'TZ': 'PST8PDT',
        # Force date strings to English.
        'LC_TIME': 'en_US.UTF-8',
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


class RefTest(object):
    """A runnable test."""
    def __init__(self, path):
        self.path = path     # str:  path of JS file relative to tests root dir
        self.options = []    # [str]: Extra options to pass to the shell
        self.jitflags = []   # [str]: JIT flags to pass to the shell

    @staticmethod
    def prefix_command(path):
        """Return the '-f shell.js' options needed to run a test with the given
        path."""
        if path == '':
            return ['-f', 'shell.js']
        head, base = os.path.split(path)
        return RefTest.prefix_command(head) \
            + ['-f', os.path.join(path, 'shell.js')]

    def get_command(self, prefix):
        dirname, filename = os.path.split(self.path)
        cmd = prefix + self.jitflags + self.options \
              + RefTest.prefix_command(dirname) + ['-f', self.path]
        return cmd


class RefTestCase(RefTest):
    """A test case consisting of a test and an expected result."""
    def __init__(self, path):
        RefTest.__init__(self, path)
        self.enable = True   # bool: True => run test, False => don't run
        self.expect = True   # bool: expected result, True => pass
        self.random = False  # bool: True => ignore output as 'random'
        self.slow = False    # bool: True => test may run slowly

        # The terms parsed to produce the above properties.
        self.terms = None

        # The tag between |...| in the test header.
        self.tag = None

        # Anything occuring after -- in the test header.
        self.comment = None

    def __str__(self):
        ans = self.path
        if not self.enable:
            ans += ', skip'
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
