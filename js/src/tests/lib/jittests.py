#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# jit_test.py -- Python harness for JavaScript trace tests.

from __future__ import print_function
import os, sys, tempfile, traceback, time
import subprocess
from subprocess import Popen, PIPE
from threading import Thread
import signal

try:
    from multiprocessing import Process, Manager, cpu_count
    HAVE_MULTIPROCESSING = True
except ImportError:
    HAVE_MULTIPROCESSING = False

from progressbar import ProgressBar, NullProgressBar

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

os.path.relpath = _relpath

class Test:
    def __init__(self, path):
        self.path = path       # path to test file

        self.jitflags = []     # jit flags to enable
        self.slow = False      # True means the test is slow-running
        self.allow_oom = False # True means that OOM is not considered a failure
        self.valgrind = False  # True means run under valgrind
        self.tz_pacific = False # True means force Pacific time for the test
        self.expect_error = '' # Errors to expect and consider passing
        self.expect_status = 0 # Exit status to expect from shell

    def copy(self):
        t = Test(self.path)
        t.jitflags = self.jitflags[:]
        t.slow = self.slow
        t.allow_oom = self.allow_oom
        t.valgrind = self.valgrind
        t.tz_pacific = self.tz_pacific
        t.expect_error = self.expect_error
        t.expect_status = self.expect_status
        return t

    COOKIE = '|jit-test|'

    @classmethod
    def from_file(cls, path, options):
        test = cls(path)

        line = open(path).readline()
        i = line.find(cls.COOKIE)
        if i != -1:
            meta = line[i + len(cls.COOKIE):].strip('\n')
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
                            test.expect_status = int(value, 0);
                        except ValueError:
                            print("warning: couldn't parse exit status %s" % value)
                    else:
                        print('warning: unrecognized |jit-test| attribute %s' % part)
                else:
                    if name == 'slow':
                        test.slow = True
                    elif name == 'allow-oom':
                        test.allow_oom = True
                    elif name == 'valgrind':
                        test.valgrind = options.valgrind
                    elif name == 'tz-pacific':
                        test.tz_pacific = True
                    elif name == 'mjitalways':
                        test.jitflags.append('--always-mjit')
                    elif name == 'debug':
                        test.jitflags.append('--debugjit')
                    elif name == 'mjit':
                        test.jitflags.append('--jm')
                    elif name == 'no-jm':
                        test.jitflags.append('--no-jm')
                    elif name == 'ion-eager':
                        test.jitflags.append('--ion-eager')
                    elif name == 'dump-bytecode':
                        test.jitflags.append('--dump-bytecode')
                    else:
                        print('warning: unrecognized |jit-test| attribute %s' % part)

        if options.valgrind_all:
            test.valgrind = True

        return test

def find_tests(dir, substring = None):
    ans = []
    for dirpath, dirnames, filenames in os.walk(dir):
        dirnames.sort()
        filenames.sort()
        if dirpath == '.':
            continue
        for filename in filenames:
            if not filename.endswith('.js'):
                continue
            if filename in ('shell.js', 'browser.js', 'jsref.js'):
                continue
            test = os.path.join(dirpath, filename)
            if substring is None or substring in os.path.relpath(test, dir):
                ans.append(test)
    return ans

def get_test_cmd(js, path, jitflags, lib_dir, shell_args):
    libdir_var = lib_dir
    if not libdir_var.endswith('/'):
        libdir_var += '/'
    scriptdir_var = os.path.dirname(path);
    if not scriptdir_var.endswith('/'):
        scriptdir_var += '/'
    expr = ("const platform=%r; const libdir=%r; const scriptdir=%r"
            % (sys.platform, libdir_var, scriptdir_var))
    # We may have specified '-a' or '-d' twice: once via --jitflags, once
    # via the "|jit-test|" line.  Remove dups because they are toggles.
    return ([js] + list(set(jitflags)) + shell_args +
            [ '-e', expr, '-f', os.path.join(lib_dir, 'prolog.js'), '-f', path ])

def tmppath(token):
    fd, path = tempfile.mkstemp(prefix=token)
    os.close(fd)
    return path

def read_and_unlink(path):
    f = open(path)
    d = f.read()
    f.close()
    os.unlink(path)
    return d

def th_run_cmd(cmdline, options, l):
    # close_fds is not supported on Windows and will cause a ValueError.
    if sys.platform != 'win32':
        options["close_fds"] = True
    p = Popen(cmdline, stdin=PIPE, stdout=PIPE, stderr=PIPE, **options)

    l[0] = p
    out, err = p.communicate()
    l[1] = (out, err, p.returncode)

def run_timeout_cmd(cmdline, options, timeout=60.0):
    l = [ None, None ]
    timed_out = False
    th = Thread(target=th_run_cmd, args=(cmdline, options, l))

    # If our SIGINT handler is set to SIG_IGN (ignore)
    # then we are running as a child process for parallel
    # execution and we must ensure to kill our child
    # when we are signaled to exit.
    sigint_handler = signal.getsignal(signal.SIGINT)
    sigterm_handler = signal.getsignal(signal.SIGTERM)
    if (sigint_handler == signal.SIG_IGN):
        def handleChildSignal(sig, frame):
            try:
                if sys.platform != 'win32':
                    os.kill(l[0].pid, signal.SIGKILL)
                else:
                    import ctypes
                    ctypes.windll.kernel32.TerminateProcess(int(l[0]._handle), -1)
            except OSError:
                pass
            if (sig == signal.SIGTERM):
                sys.exit(0)
        signal.signal(signal.SIGINT, handleChildSignal)
        signal.signal(signal.SIGTERM, handleChildSignal)

    th.start()
    th.join(timeout)
    while th.isAlive():
        if l[0] is not None:
            try:
                # In Python 3, we could just do l[0].kill().
                if sys.platform != 'win32':
                    os.kill(l[0].pid, signal.SIGKILL)
                else:
                    import ctypes
                    ctypes.windll.kernel32.TerminateProcess(int(l[0]._handle), -1)
                time.sleep(.1)
                timed_out = True
            except OSError:
                # Expecting a "No such process" error
                pass
    th.join()

    # Restore old signal handlers
    if (sigint_handler == signal.SIG_IGN):
        signal.signal(signal.SIGINT, signal.SIG_IGN)
        signal.signal(signal.SIGTERM, sigterm_handler)

    (out, err, code) = l[1]

    return (out, err, code, timed_out)

def run_cmd(cmdline, env, timeout):
    return run_timeout_cmd(cmdline, { 'env': env }, timeout)

def run_cmd_avoid_stdio(cmdline, env, timeout):
    stdoutPath, stderrPath = tmppath('jsstdout'), tmppath('jsstderr')
    env['JS_STDOUT'] = stdoutPath
    env['JS_STDERR'] = stderrPath
    _, __, code = run_timeout_cmd(cmdline, { 'env': env }, timeout)
    return read_and_unlink(stdoutPath), read_and_unlink(stderrPath), code

def run_test(test, lib_dir, shell_args, options):
    cmd = get_test_cmd(options.js_shell, test.path, test.jitflags, lib_dir, shell_args)

    if (test.valgrind and
        any([os.path.exists(os.path.join(d, 'valgrind'))
             for d in os.environ['PATH'].split(os.pathsep)])):
        valgrind_prefix = [ 'valgrind',
                            '-q',
                            '--smc-check=all-non-file',
                            '--error-exitcode=1',
                            '--gen-suppressions=all',
                            '--show-possibly-lost=no',
                            '--leak-check=full']
        if os.uname()[0] == 'Darwin':
            valgrind_prefix += ['--dsymutil=yes']
        cmd = valgrind_prefix + cmd

    if options.show_cmd:
        print(subprocess.list2cmdline(cmd))

    if options.avoid_stdio:
        run = run_cmd_avoid_stdio
    else:
        run = run_cmd

    env = os.environ.copy()
    if test.tz_pacific:
        env['TZ'] = 'PST8PDT'

    out, err, code, timed_out = run(cmd, env, options.timeout)

    if options.show_output:
        sys.stdout.write(out)
        sys.stdout.write(err)
        sys.stdout.write('Exit code: %s\n' % code)
    if test.valgrind:
        sys.stdout.write(err)
    return (check_output(out, err, code, test),
            out, err, code, timed_out)

def check_output(out, err, rc, test):
    if test.expect_error:
        return test.expect_error in err

    for line in out.split('\n'):
        if line.startswith('Trace stats check failed'):
            return False

    for line in err.split('\n'):
        if 'Assertion failed:' in line:
            return False

    if rc != test.expect_status:
        # Allow a non-zero exit code if we want to allow OOM, but only if we
        # actually got OOM.
        return test.allow_oom and 'out of memory' in err and 'Assertion failure' not in err

    return True

def print_tinderbox(label, test, message=None):
    if (test != None):
        jitflags = " ".join(test.jitflags)
        result = "%s | jit_test.py %-15s| %s" % (label, jitflags, test.path)
    else:
        result = "%s | jit_test.py " % label

    if message:
        result += ": " + message
    print(result)

def wrap_parallel_run_test(test, lib_dir, shell_args, resultQueue, options):
    # Ignore SIGINT in the child
    signal.signal(signal.SIGINT, signal.SIG_IGN)
    result = run_test(test, lib_dir, shell_args, options) + (test,)
    resultQueue.put(result)
    return result

def run_tests_parallel(tests, test_dir, lib_dir, shell_args, options):
    # This queue will contain the results of the various tests run.
    # We could make this queue a global variable instead of using
    # a manager to share, but this will not work on Windows.
    queue_manager = Manager()
    async_test_result_queue = queue_manager.Queue()

    # This queue will be used by the result process to indicate
    # that it has received a result and we can start a new process
    # on our end. The advantage is that we don't have to sleep and
    # check for worker completion ourselves regularly.
    notify_queue = queue_manager.Queue()

    # This queue will contain the return value of the function
    # processing the test results.
    result_process_return_queue = queue_manager.Queue()
    result_process = Process(target=process_test_results_parallel,
                             args=(async_test_result_queue, result_process_return_queue,
                                   notify_queue, len(tests), options, lib_dir, shell_args))
    result_process.start()

    # Ensure that a SIGTERM is handled the same way as SIGINT
    # to terminate all child processes.
    sigint_handler = signal.getsignal(signal.SIGINT)
    signal.signal(signal.SIGTERM, sigint_handler)

    worker_processes = []

    def remove_completed_workers(workers):
        new_workers = []
        for worker in workers:
            if worker.is_alive():
                new_workers.append(worker)
            else:
                worker.join()
        return new_workers

    try:
        testcnt = 0
        # Initially start as many jobs as allowed to run parallel
        for i in range(min(options.max_jobs,len(tests))):
            notify_queue.put(True)

        # For every item in the notify queue, start one new worker.
        # Every completed worker adds a new item to this queue.
        while notify_queue.get():
            if (testcnt < len(tests)):
                # Start one new worker
                worker_process = Process(target=wrap_parallel_run_test, args=(tests[testcnt], lib_dir, shell_args, async_test_result_queue, options))
                worker_processes.append(worker_process)
                worker_process.start()
                testcnt += 1

                # Collect completed workers
                worker_processes = remove_completed_workers(worker_processes)
            else:
                break

        # Wait for all processes to terminate
        while len(worker_processes) > 0:
            worker_processes = remove_completed_workers(worker_processes)

        # Signal completion to result processor, then wait for it to complete on its own
        async_test_result_queue.put(None)
        result_process.join()

        # Return what the result process has returned to us
        return result_process_return_queue.get()
    except (Exception, KeyboardInterrupt) as e:
        # Print the exception if it's not an interrupt,
        # might point to a bug or other faulty condition
        if not isinstance(e,KeyboardInterrupt):
            traceback.print_exc()

        for worker in worker_processes:
            try:
                worker.terminate()
            except:
                pass

        result_process.terminate()

    return False

def get_parallel_results(async_test_result_queue, notify_queue):
    while True:
        async_test_result = async_test_result_queue.get()

        # Check if we are supposed to terminate
        if (async_test_result == None):
            return

        # Notify parent that we got a result
        notify_queue.put(True)

        yield async_test_result

def process_test_results_parallel(async_test_result_queue, return_queue, notify_queue, num_tests, options, lib_dir, shell_args):
    gen = get_parallel_results(async_test_result_queue, notify_queue)
    ok = process_test_results(gen, num_tests, options, lib_dir, shell_args)
    return_queue.put(ok)

def print_test_summary(failures, complete, doing, options, lib_dir, shell_args):
    if failures:
        if options.write_failures:
            try:
                out = open(options.write_failures, 'w')
                # Don't write duplicate entries when we are doing multiple failures per job.
                written = set()
                for test, fout, ferr, fcode, _ in failures:
                    if test.path not in written:
                        out.write(os.path.relpath(test.path, test_dir) + '\n')
                        if options.write_failure_output:
                            out.write(fout)
                            out.write(ferr)
                            out.write('Exit code: ' + str(fcode) + "\n")
                        written.add(test.path)
                out.close()
            except IOError:
                sys.stderr.write("Exception thrown trying to write failure file '%s'\n"%
                                 options.write_failures)
                traceback.print_exc()
                sys.stderr.write('---\n')

        def show_test(test):
            if options.show_failed:
                print('    ' + subprocess.list2cmdline(get_test_cmd(options.js_shell, test.path, test.jitflags, lib_dir, shell_args)))
            else:
                print('    ' + ' '.join(test.jitflags + [test.path]))

        print('FAILURES:')
        for test, _, __, ___, timed_out in failures:
            if not timed_out:
                show_test(test)

        print('TIMEOUTS:')
        for test, _, __, ___, timed_out in failures:
            if timed_out:
                show_test(test)

        return False
    else:
        print('PASSED ALL' + ('' if complete else ' (partial run -- interrupted by user %s)' % doing))
        return True

def process_test_results(results, num_tests, options, lib_dir, shell_args):
    pb = NullProgressBar()
    if not options.hide_progress and not options.show_cmd and ProgressBar.conservative_isatty():
        fmt = [
            {'value': 'PASS',    'color': 'green'},
            {'value': 'FAIL',    'color': 'red'},
            {'value': 'TIMEOUT', 'color': 'blue'},
            {'value': 'SKIP',    'color': 'brightgray'},
        ]
        pb = ProgressBar(num_tests, fmt)

    failures = []
    timeouts = 0
    complete = False
    doing = 'before starting'
    try:
        for i, (ok, out, err, code, timed_out, test) in enumerate(results):
            doing = 'after %s'%test.path

            if not ok:
                failures.append([ test, out, err, code, timed_out ])
                pb.message("FAIL - %s" % test.path)
            if timed_out:
                timeouts += 1

            if options.tinderbox:
                if ok:
                    print_tinderbox("TEST-PASS", test);
                else:
                    lines = [ _ for _ in out.split('\n') + err.split('\n')
                              if _ != '' ]
                    if len(lines) >= 1:
                        msg = lines[-1]
                    else:
                        msg = ''
                    print_tinderbox("TEST-UNEXPECTED-FAIL", test, msg);

            n = i + 1
            pb.update(n, {
                'PASS': n - len(failures),
                'FAIL': len(failures),
                'TIMEOUT': timeouts,
                'SKIP': 0}
            )
        complete = True
    except KeyboardInterrupt:
        print_tinderbox("TEST-UNEXPECTED-FAIL", None, "Test execution interrupted by user");

    pb.finish(True)
    return print_test_summary(failures, complete, doing, options, lib_dir, shell_args)


def get_serial_results(tests, lib_dir, shell_args, options):
    for test in tests:
        result = run_test(test, lib_dir, shell_args, options)
        yield result + (test,)

def run_tests(tests, test_dir, lib_dir, shell_args, options):
    gen = get_serial_results(tests, lib_dir, shell_args, options)
    ok = process_test_results(gen, len(tests), options, lib_dir, shell_args)
    return ok

def parse_jitflags(options):
    jitflags = [ [ '-' + flag for flag in flags ]
                 for flags in options.jitflags.split(',') ]
    for flags in jitflags:
        for flag in flags:
            if flag not in ('-m', '-a', '-p', '-d', '-n'):
                print('Invalid jit flag: "%s"' % flag)
                sys.exit(1)
    return jitflags

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
