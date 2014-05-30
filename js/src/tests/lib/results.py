from __future__ import print_function

import re
from progressbar import NullProgressBar, ProgressBar
import pipes

# subprocess.list2cmdline does not properly escape for sh-like shells
def escape_cmdline(args):
    return ' '.join([ pipes.quote(a) for a in args ])

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

    def describe_failure(self):
        if self.timed_out:
            return "Timeout"
        lines = self.err.splitlines()
        for line in lines:
            # Skip the asm.js compilation success message.
            if "Successfully compiled asm.js code" not in line:
                return line
        return "Unknown"

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
    def __init__(self, options, testcount):
        self.options = options
        self.fp = options.output_fp

        self.groups = {}
        self.counts = {'PASS': 0, 'FAIL': 0, 'TIMEOUT': 0, 'SKIP': 0}
        self.n = 0

        if options.hide_progress:
            self.pb = NullProgressBar()
        else:
            fmt = [
                {'value': 'PASS',    'color': 'green'},
                {'value': 'FAIL',    'color': 'red'},
                {'value': 'TIMEOUT', 'color': 'blue'},
                {'value': 'SKIP',    'color': 'brightgray'},
            ]
            self.pb = ProgressBar(testcount, fmt)

    def push(self, output):
        if output.timed_out:
            self.counts['TIMEOUT'] += 1
        if isinstance(output, NullTestOutput):
            if self.options.tinderbox:
                self.print_tinderbox_result('TEST-KNOWN-FAIL', output.test.path, time=output.dt, skip=True)
            self.counts['SKIP'] += 1
            self.n += 1
        else:
            result = TestResult.from_output(output)
            tup = (result.result, result.test.expect, result.test.random)
            dev_label = self.LABELS[tup][1]
            if output.timed_out:
                dev_label = 'TIMEOUTS'
            self.groups.setdefault(dev_label, []).append(result.test.path)

            show = self.options.show
            if self.options.failed_only and dev_label not in ('REGRESSIONS', 'TIMEOUTS'):
                show = False
            if show:
                self.pb.beginline()

            if show:
                if self.options.show_output:
                    print('## %s: rc = %d, run time = %f' % (output.test.path, output.rc, output.dt), file=self.fp)

                if self.options.show_cmd:
                    print(escape_cmdline(output.cmd), file=self.fp)

                if self.options.show_output:
                    self.fp.write(output.out)
                    self.fp.write(output.err)

            self.n += 1

            if result.result == TestResult.PASS and not result.test.random:
                self.counts['PASS'] += 1
            elif result.test.expect and not result.test.random:
                self.counts['FAIL'] += 1
            else:
                self.counts['SKIP'] += 1

            if self.options.tinderbox:
                if len(result.results) > 1:
                    for sub_ok, msg in result.results:
                        label = self.LABELS[(sub_ok, result.test.expect, result.test.random)][0]
                        if label == 'TEST-UNEXPECTED-PASS':
                            label = 'TEST-PASS (EXPECTED RANDOM)'
                        self.print_tinderbox_result(label, result.test.path, time=output.dt, message=msg)
                self.print_tinderbox_result(self.LABELS[
                    (result.result, result.test.expect, result.test.random)][0],
                    result.test.path, time=output.dt)
                return

            if dev_label:
                def singular(label):
                    return "FIXED" if label == "FIXES" else label[:-1]
                self.pb.message("%s - %s" % (singular(dev_label), output.test.path))

        self.pb.update(self.n, self.counts)

    def finish(self, completed):
        self.pb.finish(completed)
        if not self.options.tinderbox:
            self.list(completed)

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

    def list(self, completed):
        for label, paths in sorted(self.groups.items()):
            if label == '':
                continue

            print(label)
            for path in paths:
                print('    %s' % path)

        if self.options.failure_file:
              failure_file = open(self.options.failure_file, 'w')
              if not self.all_passed():
                  if 'REGRESSIONS' in self.groups:
                      for path in self.groups['REGRESSIONS']:
                          print(path, file=failure_file)
                  if 'TIMEOUTS' in self.groups:
                      for path in self.groups['TIMEOUTS']:
                          print(path, file=failure_file)
              failure_file.close()

        suffix = '' if completed else ' (partial run -- interrupted by user)'
        if self.all_passed():
            print('PASS' + suffix)
        else:
            print('FAIL' + suffix)

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
        print(result)
