#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# jit_test.py -- Python harness for JavaScript trace tests.

from __future__ import print_function
import os, posixpath, sys, tempfile, traceback, time
import subprocess
from collections import namedtuple
import StringIO

if sys.platform.startswith('linux') or sys.platform.startswith('darwin'):
    from tasks_unix import run_all_tests
else:
    from tasks_win import run_all_tests

from progressbar import ProgressBar, NullProgressBar
from results import TestOutput

TESTS_LIB_DIR = os.path.dirname(os.path.abspath(__file__))
JS_DIR = os.path.dirname(os.path.dirname(TESTS_LIB_DIR))
TOP_SRC_DIR = os.path.dirname(os.path.dirname(JS_DIR))
TEST_DIR = os.path.join(JS_DIR, 'jit-test', 'tests')
LIB_DIR = os.path.join(JS_DIR, 'jit-test', 'lib') + os.path.sep
MODULE_DIR = os.path.join(JS_DIR, 'jit-test', 'modules') + os.path.sep
JS_CACHE_DIR = os.path.join(JS_DIR, 'jit-test', '.js-cache')
JS_TESTS_DIR = posixpath.join(JS_DIR, 'tests')

# Backported from Python 3.1 posixpath.py
def _relpath(path, start=None):
    """Return a relative version of a path"""

    if not path:
        raise ValueError("no path specified")

    if start is None:
        start = os.curdir

    start_list = os.path.abspath(start).split(os.sep)
    path_list = os.path.abspath(path).split(os.sep)

    # Work out how much of the filepath is shared by start and path.
    i = len(os.path.commonprefix([start_list, path_list]))

    rel_list = [os.pardir] * (len(start_list)-i) + path_list[i:]
    if not rel_list:
        return os.curdir
    return os.path.join(*rel_list)

# Mapping of Python chars to their javascript string representation.
QUOTE_MAP = {
    '\\': '\\\\',
    '\b': '\\b',
    '\f': '\\f',
    '\n': '\\n',
    '\r': '\\r',
    '\t': '\\t',
    '\v': '\\v'
}

# Quote the string S, javascript style.
def js_quote(quote, s):
    result = quote
    for c in s:
        if c == quote:
            result += '\\' + quote
        elif c in QUOTE_MAP:
            result += QUOTE_MAP[c]
        else:
            result += c
    result += quote
    return result

os.path.relpath = _relpath

class JitTest:

    VALGRIND_CMD = []
    paths = (d for d in os.environ['PATH'].split(os.pathsep))
    valgrinds = (os.path.join(d, 'valgrind') for d in paths)
    if any(os.path.exists(p) for p in valgrinds):
        VALGRIND_CMD = [
            'valgrind', '-q', '--smc-check=all-non-file',
            '--error-exitcode=1', '--gen-suppressions=all',
            '--show-possibly-lost=no', '--leak-check=full',
        ]
        if os.uname()[0] == 'Darwin':
            VALGRIND_CMD.append('--dsymutil=yes')

    del paths
    del valgrinds

    def __init__(self, path):
        # Absolute path of the test file.
        self.path = path

        # Path relative to the top mozilla/ directory.
        self.relpath_top = os.path.relpath(path, TOP_SRC_DIR)

        # Path relative to mozilla/js/src/jit-test/tests/.
        self.relpath_tests = os.path.relpath(path, TEST_DIR)

        self.jitflags = []     # jit flags to enable
        self.slow = False      # True means the test is slow-running
        self.allow_oom = False # True means that OOM is not considered a failure
        self.allow_unhandlable_oom = False # True means CrashAtUnhandlableOOM
                                           # is not considered a failure
        self.allow_overrecursed = False # True means that hitting recursion the
                                        # limits is not considered a failure.
        self.valgrind = False  # True means run under valgrind
        self.tz_pacific = False # True means force Pacific time for the test
        self.test_also_noasmjs = False # True means run with and without asm.js
                                       # enabled.
        self.test_also_wasm_baseline = False # True means run with and and without
                                       # wasm baseline compiler enabled.
        self.test_also = [] # List of other configurations to test with.
        self.test_join = [] # List of other configurations to test with all existing variants.
        self.expect_error = '' # Errors to expect and consider passing
        self.expect_status = 0 # Exit status to expect from shell
        self.expect_crash = False # Exit status or error output.
        self.is_module = False
        self.test_reflect_stringify = None  # Reflect.stringify implementation to test

        # Expected by the test runner. Always true for jit-tests.
        self.enable = True

    def copy(self):
        t = JitTest(self.path)
        t.jitflags = self.jitflags[:]
        t.slow = self.slow
        t.allow_oom = self.allow_oom
        t.allow_unhandlable_oom = self.allow_unhandlable_oom
        t.allow_overrecursed = self.allow_overrecursed
        t.valgrind = self.valgrind
        t.tz_pacific = self.tz_pacific
        t.test_also_noasmjs = self.test_also_noasmjs
        t.test_also_wasm_baseline = self.test_also_noasmjs
        t.test_also = self.test_also
        t.test_join = self.test_join
        t.expect_error = self.expect_error
        t.expect_status = self.expect_status
        t.expect_crash = self.expect_crash
        t.test_reflect_stringify = self.test_reflect_stringify
        t.enable = True
        t.is_module = self.is_module
        return t

    def copy_and_extend_jitflags(self, variant):
        t = self.copy()
        t.jitflags.extend(variant)
        return t

    def copy_variants(self, variants):
        # Append variants to be tested in addition to the current set of tests.
        variants = variants + self.test_also

        # For each existing variant, duplicates it for each list of options in
        # test_join.  This will multiply the number of variants by 2 for set of
        # options.
        for join_opts in self.test_join:
            variants = variants + [ opts + join_opts for opts in variants ];

        # For each list of jit flags, make a copy of the test.
        return [self.copy_and_extend_jitflags(v) for v in variants]


    COOKIE = '|jit-test|'
    CacheDir = JS_CACHE_DIR
    Directives = {}

    @classmethod
    def find_directives(cls, file_name):
        meta = ''
        line = open(file_name).readline()
        i = line.find(cls.COOKIE)
        if i != -1:
            meta = ';' + line[i + len(cls.COOKIE):].strip('\n')
        return meta

    @classmethod
    def from_file(cls, path, options):
        test = cls(path)

        # If directives.txt exists in the test's directory then it may
        # contain metainformation that will be catenated with
        # whatever's in the test file.  The form of the directive in
        # the directive file is the same as in the test file.  Only
        # the first line is considered, just as for the test file.

        dir_meta = ''
        dir_name = os.path.dirname(path)
        if dir_name in cls.Directives:
            dir_meta = cls.Directives[dir_name]
        else:
            meta_file_name = os.path.join(dir_name, "directives.txt")
            if os.path.exists(meta_file_name):
                dir_meta = cls.find_directives(meta_file_name)
            cls.Directives[dir_name] = dir_meta

        meta = cls.find_directives(path)
        if meta != '' or dir_meta != '':
            meta = meta + dir_meta
            parts = meta.split(';')
            for part in parts:
                part = part.strip()
                if not part:
                    continue
                name, _, value = part.partition(':')
                if value:
                    value = value.strip()
                    if name == 'error':
                        test.expect_error = value
                    elif name == 'exitstatus':
                        try:
                            test.expect_status = int(value, 0)
                        except ValueError:
                            print("warning: couldn't parse exit status"
                                  " {}".format(value))
                    elif name == 'thread-count':
                        try:
                            test.jitflags.append('--thread-count={}'.format(
                                int(value, 0)))
                        except ValueError:
                            print("warning: couldn't parse thread-count"
                                  " {}".format(value))
                    else:
                        print('{}: warning: unrecognized |jit-test| attribute'
                              ' {}'.format(path, part))
                else:
                    if name == 'slow':
                        test.slow = True
                    elif name == 'allow-oom':
                        test.allow_oom = True
                    elif name == 'allow-unhandlable-oom':
                        test.allow_unhandlable_oom = True
                    elif name == 'allow-overrecursed':
                        test.allow_overrecursed = True
                    elif name == 'valgrind':
                        test.valgrind = options.valgrind
                    elif name == 'tz-pacific':
                        test.tz_pacific = True
                    elif name == 'test-also-noasmjs':
                        if options.can_test_also_noasmjs:
                            test.test_also.append(['--no-asmjs'])
                            # test-also-noasmjs is a sure indicator that the file contains asm.js code;
                            # in that case we want to test the wasm baseline compiler too, as asm.js
                            # is translated to wasm
                            test.test_also.append(['--wasm-always-baseline'])
                    elif name == 'test-also-wasm-baseline':
                        if options.can_test_also_wasm_baseline:
                            test.test_also.append(['--wasm-always-baseline'])
                    elif name.startswith('test-also='):
                        test.test_also.append([name[len('test-also='):]])
                    elif name.startswith('test-join='):
                        test.test_join.append([name[len('test-join='):]])
                    elif name == 'module':
                        test.is_module = True
                    elif name == 'crash':
                        test.expect_crash = True
                    elif name.startswith('--'):
                        # // |jit-test| --ion-gvn=off; --no-sse4
                        test.jitflags.append(name)
                    else:
                        print('{}: warning: unrecognized |jit-test| attribute'
                              ' {}'.format(path, part))

        if options.valgrind_all:
            test.valgrind = True

        if options.test_reflect_stringify is not None:
            test.expect_error = ''
            test.expect_status = 0

        return test

    def command(self, prefix, libdir, moduledir, remote_prefix=None):
        path = self.path
        if remote_prefix:
            path = self.path.replace(TEST_DIR, remote_prefix)

        scriptdir_var = os.path.dirname(path)
        if not scriptdir_var.endswith('/'):
            scriptdir_var += '/'

        # Platforms where subprocess immediately invokes exec do not care
        # whether we use double or single quotes. On windows and when using
        # a remote device, however, we have to be careful to use the quote
        # style that is the opposite of what the exec wrapper uses.
        if remote_prefix:
            quotechar = '"'
        else:
            quotechar = "'"
        expr = "const platform={}; const libdir={}; const scriptdir={}".format(
            js_quote(quotechar, sys.platform),
            js_quote(quotechar, libdir),
            js_quote(quotechar, scriptdir_var))

        # We may have specified '-a' or '-d' twice: once via --jitflags, once
        # via the "|jit-test|" line.  Remove dups because they are toggles.
        cmd = prefix + ['--js-cache', JitTest.CacheDir]
        cmd += list(set(self.jitflags)) + ['-e', expr]
        if self.is_module:
            cmd += ['--module-load-path', moduledir]
            cmd += ['--module', path]
        elif self.test_reflect_stringify is None:
            cmd += ['-f', path]
        else:
            cmd += ['--', self.test_reflect_stringify, "--check", path]
        if self.valgrind:
            cmd = self.VALGRIND_CMD + cmd
        return cmd

    # The test runner expects this to be set to give to get_command.
    js_cmd_prefix = None
    def get_command(self, prefix):
        """Shim for the test runner."""
        return self.command(prefix, LIB_DIR, MODULE_DIR)


def find_tests(substring=None):
    ans = []
    for dirpath, dirnames, filenames in os.walk(TEST_DIR):
        dirnames.sort()
        filenames.sort()
        if dirpath == '.':
            continue
        for filename in filenames:
            if not filename.endswith('.js'):
                continue
            if filename in ('shell.js', 'browser.js'):
                continue
            test = os.path.join(dirpath, filename)
            if substring is None \
               or substring in os.path.relpath(test, TEST_DIR):
                ans.append(test)
    return ans

def run_test_remote(test, device, prefix, options):
    if options.test_reflect_stringify:
        raise ValueError("can't run Reflect.stringify tests remotely")
    cmd = test.command(prefix,
                       posixpath.join(options.remote_test_root, 'lib/'),
                       posixpath.join(options.remote_test_root, 'modules/'),
                       posixpath.join(options.remote_test_root, 'tests'))
    if options.show_cmd:
        print(subprocess.list2cmdline(cmd))

    env = {}
    if test.tz_pacific:
        env['TZ'] = 'PST8PDT'

    env['LD_LIBRARY_PATH'] = options.remote_test_root

    buf = StringIO.StringIO()
    returncode = device.shell(cmd, buf, env=env, cwd=options.remote_test_root,
                              timeout=int(options.timeout))

    out = buf.getvalue()
    # We can't distinguish between stdout and stderr so we pass
    # the same buffer to both.
    return TestOutput(test, cmd, out, out, returncode, None, False)

def check_output(out, err, rc, timed_out, test, options):
    if timed_out:
        if test.relpath_tests in options.ignore_timeouts:
            return True

        # The shell sometimes hangs on shutdown on Windows 7 and Windows
        # Server 2008. See bug 970063 comment 7 for a description of the
        # problem. Until bug 956899 is fixed, ignore timeouts on these
        # platforms (versions 6.0 and 6.1).
        if sys.platform == 'win32':
            ver = sys.getwindowsversion()
            if ver.major == 6 and ver.minor <= 1:
                return True
        return False

    if test.expect_error:
        # The shell exits with code 3 on uncaught exceptions.
        # Sometimes 0 is returned on Windows for unknown reasons.
        # See bug 899697.
        if sys.platform in ['win32', 'cygwin']:
            if rc != 3 and rc != 0:
                return False
        else:
            if rc != 3:
                return False

        return test.expect_error in err

    for line in out.split('\n'):
        if line.startswith('Trace stats check failed'):
            return False

    for line in err.split('\n'):
        if 'Assertion failed:' in line:
            return False

    if test.expect_crash:
        if sys.platform == 'win32' and rc == 3 - 2 ** 31:
            return True

        if sys.platform != 'win32' and rc == -11:
            return True

        # When building with ASan enabled, ASan will convert the -11 returned
        # value to 1. As a work-around we look for the error output which
        # includes the crash reason.
        if rc == 1 and ("Hit MOZ_CRASH" in err or "Assertion failure:" in err):
            return True

    if rc != test.expect_status:
        # Tests which expect a timeout check for exit code 6.
        # Sometimes 0 is returned on Windows for unknown reasons.
        # See bug 899697.
        if sys.platform in ['win32', 'cygwin'] and rc == 0:
            return True

        # Allow a non-zero exit code if we want to allow OOM, but only if we
        # actually got OOM.
        if test.allow_oom and 'out of memory' in err \
           and 'Assertion failure' not in err and 'MOZ_CRASH' not in err:
            return True

        # Allow a non-zero exit code if we want to allow unhandlable OOM, but
        # only if we actually got unhandlable OOM.
        if test.allow_unhandlable_oom \
           and 'Assertion failure: [unhandlable oom]' in err:
            return True

        # Allow a non-zero exit code if we want to all too-much-recursion and
        # the test actually over-recursed.
        if test.allow_overrecursed and 'too much recursion' in err \
           and 'Assertion failure' not in err:
            return True

        return False

    return True

def print_automation_format(ok, res):
    # Output test failures in a parsable format suitable for automation, eg:
    # TEST-RESULT | filename.js | Failure description (code N, args "--foobar")
    #
    # Example:
    # TEST-PASS | foo/bar/baz.js | (code 0, args "--ion-eager")
    # TEST-UNEXPECTED-FAIL | foo/bar/baz.js | TypeError: or something (code -9, args "--no-ion")
    # INFO exit-status     : 3
    # INFO timed-out       : False
    # INFO stdout          > foo
    # INFO stdout          > bar
    # INFO stdout          > baz
    # INFO stderr         2> TypeError: or something
    # TEST-UNEXPECTED-FAIL | jit_test.py: Test execution interrupted by user
    result = "TEST-PASS" if ok else "TEST-UNEXPECTED-FAIL"
    message = "Success" if ok else res.describe_failure()
    jitflags = " ".join(res.test.jitflags)
    print("{} | {} | {} (code {}, args \"{}\")".format(
        result, res.test.relpath_top, message, res.rc, jitflags))

    # For failed tests, print as much information as we have, to aid debugging.
    if ok:
        return
    print("INFO exit-status     : {}".format(res.rc))
    print("INFO timed-out       : {}".format(res.timed_out))
    for line in res.out.splitlines():
        print("INFO stdout          > " + line.strip())
    for line in res.err.splitlines():
        print("INFO stderr         2> " + line.strip())

def print_test_summary(num_tests, failures, complete, doing, options):
    if failures:
        if options.write_failures:
            try:
                out = open(options.write_failures, 'w')
                # Don't write duplicate entries when we are doing multiple
                # failures per job.
                written = set()
                for res in failures:
                    if res.test.path not in written:
                        out.write(os.path.relpath(res.test.path, TEST_DIR)
                                  + '\n')
                        if options.write_failure_output:
                            out.write(res.out)
                            out.write(res.err)
                            out.write('Exit code: ' + str(res.rc) + "\n")
                        written.add(res.test.path)
                out.close()
            except IOError:
                sys.stderr.write("Exception thrown trying to write failure"
                                 " file '{}'\n".format(options.write_failures))
                traceback.print_exc()
                sys.stderr.write('---\n')

        def show_test(res):
            if options.show_failed:
                print('    ' + subprocess.list2cmdline(res.cmd))
            else:
                print('    ' + ' '.join(res.test.jitflags + [res.test.path]))

        print('FAILURES:')
        for res in failures:
            if not res.timed_out:
                show_test(res)

        print('TIMEOUTS:')
        for res in failures:
            if res.timed_out:
                show_test(res)
    else:
        print('PASSED ALL'
              + ('' if complete
                 else ' (partial run -- interrupted by user {})'.format(doing)))

    if options.format == 'automation':
        num_failures = len(failures) if failures else 0
        print('Result summary:')
        print('Passed: {:d}'.format(num_tests - num_failures))
        print('Failed: {:d}'.format(num_failures))

    return not failures

def create_progressbar(num_tests, options):
    if not options.hide_progress and not options.show_cmd \
       and ProgressBar.conservative_isatty():
        fmt = [
            {'value': 'PASS',    'color': 'green'},
            {'value': 'FAIL',    'color': 'red'},
            {'value': 'TIMEOUT', 'color': 'blue'},
            {'value': 'SKIP',    'color': 'brightgray'},
        ]
        return ProgressBar(num_tests, fmt)
    return NullProgressBar()

def process_test_results(results, num_tests, pb, options):
    failures = []
    timeouts = 0
    complete = False
    output_dict = {}
    doing = 'before starting'

    if num_tests == 0:
        pb.finish(True)
        complete = True
        return print_test_summary(num_tests, failures, complete, doing, options)

    try:
        for i, res in enumerate(results):
            ok = check_output(res.out, res.err, res.rc, res.timed_out,
                              res.test, options)

            if ok:
                show_output = options.show_output and not options.failed_only
            else:
                show_output = options.show_output or not options.no_show_failed

            if show_output:
                pb.beginline()
                sys.stdout.write(res.out)
                sys.stdout.write(res.err)
                sys.stdout.write('Exit code: {}\n'.format(res.rc))

            if res.test.valgrind and not show_output:
                pb.beginline()
                sys.stdout.write(res.err)

            if options.check_output:
                if res.test.path in output_dict.keys():
                    if output_dict[res.test.path] != res.out:
                        pb.message("FAIL - OUTPUT DIFFERS {}".format(res.test.relpath_tests))
                else:
                    output_dict[res.test.path] = res.out

            doing = 'after {}'.format(res.test.relpath_tests)
            if not ok:
                failures.append(res)
                if res.timed_out:
                    pb.message("TIMEOUT - {}".format(res.test.relpath_tests))
                    timeouts += 1
                else:
                    pb.message("FAIL - {}".format(res.test.relpath_tests))

            if options.format == 'automation':
                print_automation_format(ok, res)

            n = i + 1
            pb.update(n, {
                'PASS': n - len(failures),
                'FAIL': len(failures),
                'TIMEOUT': timeouts,
                'SKIP': 0
            })
        complete = True
    except KeyboardInterrupt:
        print("TEST-UNEXPECTED-FAIL | jit_test.py" +
              " : Test execution interrupted by user")

    pb.finish(True)
    return print_test_summary(num_tests, failures, complete, doing, options)

def run_tests(tests, num_tests, prefix, options):
    # The jstests tasks runner requires the following options. The names are
    # taken from the jstests options processing code, which are frequently
    # subtly different from the options jit-tests expects. As such, we wrap
    # them here, as needed.
    AdaptorOptions = namedtuple("AdaptorOptions", [
        "worker_count", "passthrough", "timeout", "output_fp",
        "hide_progress", "run_skipped", "show_cmd"])
    shim_options = AdaptorOptions(options.max_jobs, False, options.timeout,
                                  sys.stdout, False, True, options.show_cmd)

    # The test runner wants the prefix as a static on the Test class.
    JitTest.js_cmd_prefix = prefix

    pb = create_progressbar(num_tests, options)
    gen = run_all_tests(tests, prefix, pb, shim_options)
    ok = process_test_results(gen, num_tests, pb, options)
    return ok

def get_remote_results(tests, device, prefix, options):
    from mozdevice import devicemanager

    try:
        for i in xrange(0, options.repeat):
            for test in tests:
                yield run_test_remote(test, device, prefix, options)
    except devicemanager.DMError as e:
        # After a devicemanager error, the device is typically in a
        # state where all further tests will fail so there is no point in
        # continuing here.
        sys.stderr.write("Error running remote tests: {}".format(e.message))

def push_libs(options, device):
    # This saves considerable time in pushing unnecessary libraries
    # to the device but needs to be updated if the dependencies change.
    required_libs = ['libnss3.so', 'libmozglue.so', 'libnspr4.so',
                     'libplc4.so', 'libplds4.so']

    for file in os.listdir(options.local_lib):
        if file in required_libs:
            remote_file = posixpath.join(options.remote_test_root, file)
            device.pushFile(os.path.join(options.local_lib, file), remote_file)

def push_progs(options, device, progs):
    for local_file in progs:
        remote_file = posixpath.join(options.remote_test_root,
                                     os.path.basename(local_file))
        device.pushFile(local_file, remote_file)

def run_tests_remote(tests, num_tests, prefix, options):
    # Setup device with everything needed to run our tests.
    from mozdevice import devicemanagerADB, devicemanagerSUT

    if options.device_transport == 'adb':
        if options.device_ip:
            dm = devicemanagerADB.DeviceManagerADB(
                options.device_ip, options.device_port,
                deviceSerial=options.device_serial,
                packageName=None,
                deviceRoot=options.remote_test_root)
        else:
            dm = devicemanagerADB.DeviceManagerADB(
                deviceSerial=options.device_serial,
                packageName=None,
                deviceRoot=options.remote_test_root)
    else:
        dm = devicemanagerSUT.DeviceManagerSUT(
            options.device_ip, options.device_port,
            deviceRoot=options.remote_test_root)
        if options.device_ip == None:
            print('Error: you must provide a device IP to connect to via the'
                  ' --device option')
            sys.exit(1)

    # Update the test root to point to our test directory.
    jit_tests_dir = posixpath.join(options.remote_test_root, 'jit-tests')
    options.remote_test_root = posixpath.join(jit_tests_dir, 'jit-tests')

    # Push js shell and libraries.
    if dm.dirExists(jit_tests_dir):
        dm.removeDir(jit_tests_dir)
    dm.mkDirs(options.remote_test_root)
    push_libs(options, dm)
    push_progs(options, dm, [prefix[0]])
    dm.chmodDir(options.remote_test_root)

    JitTest.CacheDir = posixpath.join(options.remote_test_root, '.js-cache')
    dm.mkDir(JitTest.CacheDir)

    dm.pushDir(JS_TESTS_DIR, posixpath.join(jit_tests_dir, 'tests'),
               timeout=600)

    dm.pushDir(os.path.dirname(TEST_DIR), options.remote_test_root,
               timeout=600)
    prefix[0] = os.path.join(options.remote_test_root, 'js')

    # Run all tests.
    pb = create_progressbar(num_tests, options)
    gen = get_remote_results(tests, dm, prefix, options)
    ok = process_test_results(gen, num_tests, pb, options)
    return ok

def platform_might_be_android():
    try:
        # The python package for SL4A provides an |android| module.
        # If that module is present, we're likely in SL4A-python on
        # device.  False positives and negatives are possible,
        # however.
        import android
        return True
    except ImportError:
        return False

def stdio_might_be_broken():
    return platform_might_be_android()

if __name__ == '__main__':
    print('Use ../jit-test/jit_test.py to run these tests.')
