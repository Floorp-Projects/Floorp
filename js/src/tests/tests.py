# Library for JSTest tests.
#
# This contains classes that represent an individual test, including
# metadata, and know how to run the tests and determine failures.

import datetime, os, re, sys, time
from subprocess import *
from threading import *

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
        dir, filename = os.path.split(self.path)
        cmd = js_cmd_prefix + Test.prefix_command(dir)
        if self.debugMode:
            cmd += [ '-d' ]
        # There is a test that requires the path to start with './'.
        cmd += [ '-f', './' + self.path ]
        return cmd

    def run(self, js_cmd_prefix, timeout=30.0):
        cmd = self.get_command(js_cmd_prefix)
        out, err, rc, dt, timed_out = run_cmd(cmd, timeout)
        return TestOutput(self, cmd, out, err, rc, dt, timed_out)

class TestCase(Test):
    """A test case consisting of a test and an expected result."""

    def __init__(self, path, enable, expect, random, slow, debugMode):
        Test.__init__(self, path)
        self.enable = enable     # bool: True => run test, False => don't run
        self.expect = expect     # bool: expected result, True => pass
        self.random = random     # bool: True => ignore output as 'random'
        self.slow = slow         # bool: True => test may run slowly
        self.debugMode = debugMode # bool: True => must be run in debug mode

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
        if self.debugMode:
            ans += ', debugMode'
        return ans

class TestOutput:
    """Output from a test run."""
    def __init__(self, test, cmd, out, err, rc, dt, timed_out):
        self.test = test   # Test
        self.cmd = cmd     # str:   command line of test
        self.out = out     # str:   stdout
        self.err = err     # str:   stderr
        self.rc = rc       # int:   return code
        self.dt = dt       # float: run time
        self.timed_out = timed_out # bool: did the test time out

class NullTestOutput:
    """Variant of TestOutput that indicates a test was not run."""
    def __init__(self, test):
        self.test = test
        self.cmd = ''
        self.out = ''
        self.err = ''
        self.rc = 0
        self.dt = 0.0
        self.timed_out = False

class TestResult:
    PASS = 'PASS'
    FAIL = 'FAIL'
    CRASH = 'CRASH'

    """Classified result from a test run."""
    def __init__(self, test, result, results):
        self.test = test
        self.result = result
        self.results = results

    @classmethod
    def from_output(cls, output):
        test = output.test
        result = None          # str:      overall result, see class-level variables
        results = []           # (str,str) list: subtest results (pass/fail, message)

        out, rc = output.out, output.rc

        failures = 0
        passes = 0

        expected_rcs = []
        if test.path.endswith('-n.js'):
            expected_rcs.append(3)

        for line in out.split('\n'):
            if line.startswith(' FAILED!'):
                failures += 1
                msg = line[len(' FAILED! '):]
                results.append((cls.FAIL, msg))
            elif line.startswith(' PASSED!'):
                passes += 1
                msg = line[len(' PASSED! '):]
                results.append((cls.PASS, msg))
            else:
                m = re.match('--- NOTE: IN THIS TESTCASE, WE EXPECT EXIT CODE ((?:-|\\d)+) ---', line)
                if m:
                    expected_rcs.append(int(m.group(1)))

        if rc and not rc in expected_rcs:
            if rc == 3:
                result = cls.FAIL
            else:
                result = cls.CRASH
        else:
            if (rc or passes > 0) and failures == 0:
                result = cls.PASS
            else:
                result = cls.FAIL

        return cls(test, result, results)
