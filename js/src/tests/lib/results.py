from __future__ import print_function

import json
import pipes
import re

from progressbar import NullProgressBar, ProgressBar
from structuredlog import TestLogger

# subprocess.list2cmdline does not properly escape for sh-like shells


def escape_cmdline(args):
    return ' '.join([pipes.quote(a) for a in args])


class TestOutput:
    """Output from a test run."""
    def __init__(self, test, cmd, out, err, rc, dt, timed_out, extra=None):
        self.test = test   # Test
        self.cmd = cmd     # str:   command line of test
        self.out = out     # str:   stdout
        self.err = err     # str:   stderr
        self.rc = rc       # int:   return code
        self.dt = dt       # float: run time
        self.timed_out = timed_out  # bool: did the test time out
        self.extra = extra  # includes the pid on some platforms

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
    def from_wpt_output(cls, output):
        """Parse the output from a web-platform test that uses testharness.js.
        (The output is written to stdout in js/src/tests/testharnessreport.js.)
        """
        from wptrunner.executors.base import testharness_result_converter

        rc = output.rc
        stdout = output.out.split("\n")
        if rc != 0:
            if rc == 3:
                harness_status = "ERROR"
                harness_message = "Exit code reported exception"
            else:
                harness_status = "CRASH"
                harness_message = "Exit code reported crash"
            tests = []
        else:
            for (idx, line) in enumerate(stdout):
                if line.startswith("WPT OUTPUT: "):
                    msg = line[len("WPT OUTPUT: "):]
                    data = [output.test.wpt.url] + json.loads(msg)
                    harness_status_obj, tests = testharness_result_converter(output.test.wpt, data)
                    harness_status = harness_status_obj.status
                    harness_message = "Reported by harness: %s" % (harness_status_obj.message,)
                    del stdout[idx]
                    break
            else:
                harness_status = "ERROR"
                harness_message = "No harness output found"
                tests = []
        stdout.append("Harness status: %s (%s)" % (harness_status, harness_message))

        result = cls.PASS
        results = []
        if harness_status != output.test.wpt.expected():
            if harness_status == "CRASH":
                result = cls.CRASH
            else:
                result = cls.FAIL
        else:
            for test in tests:
                test_output = "Subtest \"%s\": " % (test.name,)
                expected = output.test.wpt.expected(test.name)
                if test.status == expected:
                    test_result = (cls.PASS, "")
                    test_output += "as expected: %s" % (test.status,)
                else:
                    test_result = (cls.FAIL, test.message)
                    result = cls.FAIL
                    test_output += "expected %s, found %s" % (expected, test.status)
                    if test.message:
                        test_output += " (with message: \"%s\")" % (test.message,)
                results.append(test_result)
                stdout.append(test_output)

        output.out = "\n".join(stdout) + "\n"

        return cls(output.test, result, results)

    @classmethod
    def from_output(cls, output):
        test = output.test
        result = None          # str:      overall result, see class-level variables
        results = []           # (str,str) list: subtest results (pass/fail, message)

        if test.wpt:
            return cls.from_wpt_output(output)

        out, err, rc = output.out, output.err, output.rc

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
                m = re.match('--- NOTE: IN THIS TESTCASE, WE EXPECT EXIT CODE'
                             ' ((?:-|\\d)+) ---', line)
                if m:
                    expected_rcs.append(int(m.group(1)))

        if test.error is not None:
            expected_rcs.append(3)
            if test.error not in err:
                failures += 1
                results.append((cls.FAIL, "Expected uncaught error: {}".format(test.error)))

        if rc and rc not in expected_rcs:
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


class TestDuration:
    def __init__(self, test, duration):
        self.test = test
        self.duration = duration


class ResultsSink:
    def __init__(self, testsuite, options, testcount):
        self.options = options
        self.fp = options.output_fp
        if self.options.format == 'automation':
            self.slog = TestLogger(testsuite)
            self.slog.suite_start()

        self.groups = {}
        self.output_dict = {}
        self.counts = {'PASS': 0, 'FAIL': 0, 'TIMEOUT': 0, 'SKIP': 0}
        self.slow_tests = []
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
        if self.options.show_slow and output.dt >= self.options.slow_test_threshold:
            self.slow_tests.append(TestDuration(output.test, output.dt))
        if output.timed_out:
            self.counts['TIMEOUT'] += 1
        if isinstance(output, NullTestOutput):
            if self.options.format == 'automation':
                self.print_automation_result(
                    'TEST-KNOWN-FAIL', output.test, time=output.dt,
                    skip=True)
            self.counts['SKIP'] += 1
            self.n += 1
        else:
            result = TestResult.from_output(output)
            tup = (result.result, result.test.expect, result.test.random)
            dev_label = self.LABELS[tup][1]

            if self.options.check_output:
                if output.test.path in self.output_dict.keys():
                    if self.output_dict[output.test.path] != output:
                        self.counts['FAIL'] += 1
                        self.print_automation_result(
                            "TEST-UNEXPECTED-FAIL", result.test, time=output.dt,
                            message="Same test with different flag producing different output")
                else:
                    self.output_dict[output.test.path] = output

            if output.timed_out:
                dev_label = 'TIMEOUTS'
            self.groups.setdefault(dev_label, []).append(result)

            if dev_label == 'REGRESSIONS':
                show_output = self.options.show_output \
                    or not self.options.no_show_failed
            elif dev_label == 'TIMEOUTS':
                show_output = self.options.show_output
            else:
                show_output = self.options.show_output \
                    and not self.options.failed_only

            if dev_label in ('REGRESSIONS', 'TIMEOUTS'):
                show_cmd = self.options.show_cmd
            else:
                show_cmd = self.options.show_cmd \
                    and not self.options.failed_only

            if show_output or show_cmd:
                self.pb.beginline()

                if show_output:
                    print('## {}: rc = {:d}, run time = {}'.format(
                        output.test.path, output.rc, output.dt), file=self.fp)

                if show_cmd:
                    print(escape_cmdline(output.cmd), file=self.fp)

                if show_output:
                    self.fp.write(output.out)
                    self.fp.write(output.err)

            self.n += 1

            if result.result == TestResult.PASS and not result.test.random:
                self.counts['PASS'] += 1
            elif result.test.expect and not result.test.random:
                self.counts['FAIL'] += 1
            else:
                self.counts['SKIP'] += 1

            if self.options.format == 'automation':
                if result.result != TestResult.PASS and len(result.results) > 1:
                    for sub_ok, msg in result.results:
                        tup = (sub_ok, result.test.expect, result.test.random)
                        label = self.LABELS[tup][0]
                        if label == 'TEST-UNEXPECTED-PASS':
                            label = 'TEST-PASS (EXPECTED RANDOM)'
                        self.print_automation_result(
                            label, result.test, time=output.dt,
                            message=msg)
                tup = (result.result, result.test.expect, result.test.random)
                self.print_automation_result(self.LABELS[tup][0],
                                             result.test,
                                             time=output.dt,
                                             extra=getattr(output, 'extra', None))
                return

            if dev_label:
                def singular(label):
                    return "FIXED" if label == "FIXES" else label[:-1]
                self.pb.message("{} - {}".format(singular(dev_label),
                                                 output.test.path))

        self.pb.update(self.n, self.counts)

    def finish(self, completed):
        self.pb.finish(completed)
        if self.options.format == 'automation':
            self.slog.suite_end()
        else:
            self.list(completed)

    # Conceptually, this maps (test result x test expection) to text labels.
    #      key   is (result, expect, random)
    #      value is (automation label, dev test category)
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
        for label, results in sorted(self.groups.items()):
            if label == '':
                continue

            print(label)
            for result in results:
                print('    {}'.format(' '.join(result.test.jitflags +
                                               [result.test.path])))

        if self.options.failure_file:
            failure_file = open(self.options.failure_file, 'w')
            if not self.all_passed():
                if 'REGRESSIONS' in self.groups:
                    for result in self.groups['REGRESSIONS']:
                        print(result.test.path, file=failure_file)
                if 'TIMEOUTS' in self.groups:
                    for result in self.groups['TIMEOUTS']:
                        print(result.test.path, file=failure_file)
            failure_file.close()

        suffix = '' if completed else ' (partial run -- interrupted by user)'
        if self.all_passed():
            print('PASS' + suffix)
        else:
            print('FAIL' + suffix)

        if self.options.show_slow:
            min_duration = self.options.slow_test_threshold
            print('Slow tests (duration > {}s)'.format(min_duration))
            slow_tests = sorted(self.slow_tests, key=lambda x: x.duration, reverse=True)
            any = False
            for test in slow_tests:
                print('{:>5} {}'.format(round(test.duration, 2), test.test))
                any = True
            if not any:
                print('None')

    def all_passed(self):
        return 'REGRESSIONS' not in self.groups and 'TIMEOUTS' not in self.groups

    def print_automation_result(self, label, test, message=None, skip=False,
                                time=None, extra=None):
        result = label
        result += " | " + test.path
        args = []
        if self.options.shell_args:
            args.append(self.options.shell_args)
        args += test.jitflags
        result += ' | (args: "{}")'.format(' '.join(args))
        if message:
            result += " | " + message
        if skip:
            result += ' | (SKIP)'
        if time > self.options.timeout:
            result += ' | (TIMEOUT)'
        result += ' [{:.1f} s]'.format(time)
        print(result)

        details = {'extra': extra.copy() if extra else {}}
        if self.options.shell_args:
            details['extra']['shell_args'] = self.options.shell_args
        details['extra']['jitflags'] = test.jitflags
        if message:
            details['message'] = message
        status = 'FAIL' if 'TEST-UNEXPECTED' in label else 'PASS'

        self.slog.test(test.path, status, time or 0, **details)
