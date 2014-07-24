# Library for JSTest tests.
#
# This contains classes that represent an individual test, including
# metadata, and know how to run the tests and determine failures.

import datetime, os, sys, time
from subprocess import Popen, PIPE
from threading import Thread

from results import TestOutput

# When run on tbpl, we run each test multiple times with the following arguments.
TBPL_FLAGS = [
    [], # no flags, normal baseline and ion
    ['--ion-eager', '--ion-offthread-compile=off'], # implies --baseline-eager
    ['--ion-eager', '--ion-offthread-compile=off', '--ion-check-range-analysis', '--no-sse3', '--no-threads'],
    ['--baseline-eager'],
    ['--baseline-eager', '--no-fpu'],
    ['--no-baseline', '--no-ion'],
]

def do_run_cmd(cmd):
    l = [ None, None ]
    th_run_cmd(cmd, l)
    return l[1]

def set_limits():
    # resource module not supported on all platforms
    try:
        import resource
        GB = 2**30
        resource.setrlimit(resource.RLIMIT_AS, (2*GB, 2*GB))
    except:
        return

def th_run_cmd(cmd, l):
    t0 = datetime.datetime.now()

    # close_fds and preexec_fn are not supported on Windows and will
    # cause a ValueError.
    options = {}
    if sys.platform != 'win32':
        options["close_fds"] = True
        options["preexec_fn"] = set_limits
    p = Popen(cmd, stdin=PIPE, stdout=PIPE, stderr=PIPE, **options)

    l[0] = p
    out, err = p.communicate()
    t1 = datetime.datetime.now()
    dd = t1-t0
    dt = dd.seconds + 1e-6 * dd.microseconds
    l[1] = (out, err, p.returncode, dt)

def run_cmd(cmd, timeout=60.0):
    if timeout is None:
        return do_run_cmd(cmd)

    l = [ None, None ]
    timed_out = False
    th = Thread(target=th_run_cmd, args=(cmd, l))
    th.start()
    th.join(timeout)
    while th.isAlive():
        if l[0] is not None:
            try:
                # In Python 3, we could just do l[0].kill().
                import signal
                if sys.platform != 'win32':
                    os.kill(l[0].pid, signal.SIGKILL)
                time.sleep(.1)
                timed_out = True
            except OSError:
                # Expecting a "No such process" error
                pass
    th.join()
    return l[1] + (timed_out,)

class Test(object):
    """A runnable test."""
    def __init__(self, path):
        self.path = path         # str:  path of JS file relative to tests root dir

    @staticmethod
    def prefix_command(path):
        """Return the '-f shell.js' options needed to run a test with the given path."""
        if path == '':
            return [ '-f', 'shell.js' ]
        head, base = os.path.split(path)
        return Test.prefix_command(head) + [ '-f', os.path.join(path, 'shell.js') ]

    def get_command(self, js_cmd_prefix):
        dirname, filename = os.path.split(self.path)
        cmd = js_cmd_prefix + self.options + Test.prefix_command(dirname) + [ '-f', self.path ]
        return cmd

    def run(self, js_cmd_prefix, timeout=30.0):
        cmd = self.get_command(js_cmd_prefix)
        out, err, rc, dt, timed_out = run_cmd(cmd, timeout)
        return TestOutput(self, cmd, out, err, rc, dt, timed_out)

class TestCase(Test):
    """A test case consisting of a test and an expected result."""
    js_cmd_prefix = None

    def __init__(self, path):
        Test.__init__(self, path)
        self.enable = True   # bool: True => run test, False => don't run
        self.expect = True   # bool: expected result, True => pass
        self.random = False  # bool: True => ignore output as 'random'
        self.slow = False    # bool: True => test may run slowly
        self.options = []    # [str]: Extra options to pass to the shell

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

    @classmethod
    def set_js_cmd_prefix(self, js_path, js_args, debugger_prefix):
        parts = []
        if debugger_prefix:
            parts += debugger_prefix
        parts.append(js_path)
        if js_args:
            parts += js_args
        self.js_cmd_prefix = parts

    def __cmp__(self, other):
        if self.path == other.path:
            return 0
        elif self.path < other.path:
            return -1
        return 1

    def __hash__(self):
        return self.path.__hash__()
