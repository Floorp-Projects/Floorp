import re
from subprocess import list2cmdline

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

class ResultsSink:
    def __init__(self, output_file, options):
        self.output_file = output_file
        self.options = options

        self.groups = {}
        self.counts = [ 0, 0, 0 ]
        self.n = 0

        self.finished = False
        self.pb = None

    def push(self, output):
        if isinstance(output, NullTestOutput):
            if self.options.tinderbox:
                print_tinderbox_result('TEST-KNOWN-FAIL', output.test.path, time=output.dt, skip=True)
            self.counts[2] += 1
            self.n += 1
        else:
            if self.options.show_cmd:
                print >> self.output_file, list2cmdline(output.cmd)

            if self.options.show_output:
                print >> self.output_file, '    rc = %d, run time = %f' % (output.rc, output.dt)
                self.output_file.write(output.out)
                self.output_file.write(output.err)

            result = TestResult.from_output(output)
            tup = (result.result, result.test.expect, result.test.random)
            dev_label = self.LABELS[tup][1]
            if output.timed_out:
                dev_label = 'TIMEOUTS'
            self.groups.setdefault(dev_label, []).append(result.test.path)

            self.n += 1

            if result.result == TestResult.PASS and not result.test.random:
                self.counts[0] += 1
            elif result.test.expect and not result.test.random:
                self.counts[1] += 1
            else:
                self.counts[2] += 1

            if self.options.tinderbox:
                if len(result.results) > 1:
                    for sub_ok, msg in result.results:
                        label = self.LABELS[(sub_ok, result.test.expect, result.test.random)][0]
                        if label == 'TEST-UNEXPECTED-PASS':
                            label = 'TEST-PASS (EXPECTED RANDOM)'
                        print_tinderbox_result(label, result.test.path, time=output.dt, message=msg)
                print_tinderbox_result(self.LABELS[
                    (result.result, result.test.expect, result.test.random)][0],
                    result.test.path, time=output.dt)

        if self.pb:
            self.pb.label = '[%4d|%4d|%4d]'%tuple(self.counts)
            self.pb.update(self.n)

    # Conceptually, this maps (test result x test expection) to text labels.
    #      key   is (result, expect, random)
    #      value is (tinderbox label, dev test category)
    LABELS = {
        (TestResult.CRASH, False, False): ('TEST-UNEXPECTED-FAIL',               'REGRESSIONS'),
        (TestResult.CRASH, False, True):  ('TEST-UNEXPECTED-FAIL',               'REGRESSIONS'),
        (TestResult.CRASH, True,  False): ('TEST-UNEXPECTED-FAIL',               'REGRESSIONS'),
        (TestResult.CRASH, True,  True):  ('TEST-UNEXPECTED-FAIL',               'REGRESSIONS'),

        (TestResult.FAIL,  False, False): ('TEST-KNOWN-FAIL',                    ''),
        (TestResult.FAIL,  False, True):  ('TEST-KNOWN-FAIL (EXPECTED RANDOM)',  ''),
        (TestResult.FAIL,  True,  False): ('TEST-UNEXPECTED-FAIL',               'REGRESSIONS'),
        (TestResult.FAIL,  True,  True):  ('TEST-KNOWN-FAIL (EXPECTED RANDOM)',  ''),

        (TestResult.PASS,  False, False): ('TEST-UNEXPECTED-PASS',               'FIXES'),
        (TestResult.PASS,  False, True):  ('TEST-PASS (EXPECTED RANDOM)',        ''),
        (TestResult.PASS,  True,  False): ('TEST-PASS',                          ''),
        (TestResult.PASS,  True,  True):  ('TEST-PASS (EXPECTED RANDOM)',        ''),
        }

    def list(self):
        for label, paths in sorted(self.groups.items()):
            if label == '': continue

            print label
            for path in paths:
                print '    %s'%path

        if self.options.failure_file:
              failure_file = open(self.options.failure_file, 'w')
              if not self.all_passed():
                  for path in self.groups['REGRESSIONS'] + self.groups['TIMEOUTS']:
                      print >> failure_file, path
              failure_file.close()

        suffix = '' if self.finished else ' (partial run -- interrupted by user)'
        if self.all_passed():
            print 'PASS' + suffix
        else:
            print 'FAIL' + suffix

    def all_passed(self):
        return 'REGRESSIONS' not in self.groups and 'TIMEOUTS' not in self.groups

    def print_tinderbox_result(self, label, path, message=None, skip=False, time=None):
        result = label
        result += " | " + path
        result += " |" + self.options.shell_args
        if message:
            result += " | " + message
        if skip:
            result += ' | (SKIP)'
        if time > self.options.timeout:
            result += ' | (TIMEOUT)'
        print result

