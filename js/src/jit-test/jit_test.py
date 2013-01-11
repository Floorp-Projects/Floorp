#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# jit_test.py -- Python harness for JavaScript trace tests.

import datetime, os, re, sys, tempfile, traceback, time, shlex
import subprocess
from subprocess import *
from threading import Thread
import signal

try:
    from multiprocessing import Process, Queue, Manager, cpu_count
    HAVE_MULTIPROCESSING = True
except ImportError:
    HAVE_MULTIPROCESSING = False


def add_libdir_to_path():
    from os.path import dirname, exists, join, realpath
    js_src_dir = dirname(dirname(realpath(sys.argv[0])))
    assert exists(join(js_src_dir,'jsapi.h'))
    sys.path.append(join(js_src_dir, 'lib'))
    sys.path.append(join(js_src_dir, 'tests', 'lib'))

add_libdir_to_path()
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
                            print("warning: couldn't parse exit status %s"%value)
                    else:
                        print('warning: unrecognized |jit-test| attribute %s'%part)
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
                        test.jitflags.append('-a')
                    elif name == 'debug':
                        test.jitflags.append('-d')
                    elif name == 'mjit':
                        test.jitflags.append('-m')
                    elif name == 'ion-eager':
                        test.jitflags.append('--ion-eager')
                    elif name == 'dump-bytecode':
                        test.jitflags.append('-D')
                    else:
                        print('warning: unrecognized |jit-test| attribute %s'%part)

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

def get_test_cmd(path, jitflags, lib_dir, shell_args):
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
    return ([ JS ] + list(set(jitflags)) + shell_args +
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
    import signal
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
                import signal
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

def run_test(test, lib_dir, shell_args):
    cmd = get_test_cmd(test.path, test.jitflags, lib_dir, shell_args)

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

    if OPTIONS.show_cmd:
        print(subprocess.list2cmdline(cmd))

    if OPTIONS.avoid_stdio:
        run = run_cmd_avoid_stdio
    else:
        run = run_cmd

    env = os.environ.copy()
    if test.tz_pacific:
        env['TZ'] = 'PST8PDT'

    out, err, code, timed_out = run(cmd, env, OPTIONS.timeout)

    if OPTIONS.show_output:
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
    print result

def wrap_parallel_run_test(test, lib_dir, shell_args, resultQueue, options, js):
    # This is necessary because on Windows global variables are not automatically
    # available in the children, while on Linux and OSX this is the case (because of fork).
    global OPTIONS
    global JS
    OPTIONS = options
    JS = js

    # Ignore SIGINT in the child
    signal.signal(signal.SIGINT, signal.SIG_IGN)

    result = run_test(test, lib_dir, shell_args) + (test,)
    resultQueue.put(result)
    return result

def run_tests_parallel(tests, test_dir, lib_dir, shell_args):
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
                                   notify_queue, len(tests), OPTIONS, JS, lib_dir, shell_args))
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
        for i in range(min(OPTIONS.max_jobs,len(tests))):
            notify_queue.put(True)

        # For every item in the notify queue, start one new worker.
        # Every completed worker adds a new item to this queue.
        while notify_queue.get():
            if (testcnt < len(tests)):
                # Start one new worker
                worker_process = Process(target=wrap_parallel_run_test, args=(tests[testcnt], lib_dir, shell_args, async_test_result_queue, OPTIONS, JS))
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

def process_test_results_parallel(async_test_result_queue, return_queue, notify_queue, num_tests, options, js, lib_dir, shell_args):
    gen = get_parallel_results(async_test_result_queue, notify_queue)
    ok = process_test_results(gen, num_tests, options, js, lib_dir, shell_args)
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
                print('    ' + subprocess.list2cmdline(get_test_cmd(test.path, test.jitflags, lib_dir, shell_args)))
            else:
                print('    ' + ' '.join(test.jitflags + [ test.path ]))

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
        print('PASSED ALL' + ('' if complete else ' (partial run -- interrupted by user %s)'%doing))
        return True

def process_test_results(results, num_tests, options, js, lib_dir, shell_args):
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


def get_serial_results(tests, lib_dir, shell_args):
    for test in tests:
        result = run_test(test, lib_dir, shell_args)
        yield result + (test,)

def run_tests(tests, test_dir, lib_dir, shell_args):
    gen = get_serial_results(tests, lib_dir, shell_args)
    ok = process_test_results(gen, len(tests), OPTIONS, JS, lib_dir, shell_args)
    return ok

def parse_jitflags():
    jitflags = [ [ '-' + flag for flag in flags ]
                 for flags in OPTIONS.jitflags.split(',') ]
    for flags in jitflags:
        for flag in flags:
            if flag not in ('-m', '-a', '-p', '-d', '-n'):
                print('Invalid jit flag: "%s"'%flag)
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

JS = None
OPTIONS = None
def main(argv):
    global JS, OPTIONS

    script_path = os.path.abspath(__file__)
    script_dir = os.path.dirname(script_path)
    test_dir = os.path.join(script_dir, 'tests')
    lib_dir = os.path.join(script_dir, 'lib')

    # If no multiprocessing is available, fallback to serial test execution
    max_jobs_default = 1
    if HAVE_MULTIPROCESSING:
        try:
            max_jobs_default = cpu_count()
            print "Defaulting to %d jobs in parallel" % max_jobs_default
        except NotImplementedError:
            pass

    # The [TESTS] optional arguments are paths of test files relative
    # to the jit-test/tests directory.

    from optparse import OptionParser
    op = OptionParser(usage='%prog [options] JS_SHELL [TESTS]')
    op.add_option('-s', '--show-cmd', dest='show_cmd', action='store_true',
                  help='show js shell command run')
    op.add_option('-f', '--show-failed-cmd', dest='show_failed',
                  action='store_true', help='show command lines of failed tests')
    op.add_option('-o', '--show-output', dest='show_output', action='store_true',
                  help='show output from js shell')
    op.add_option('-x', '--exclude', dest='exclude', action='append',
                  help='exclude given test dir or path')
    op.add_option('--no-slow', dest='run_slow', action='store_false',
                  help='do not run tests marked as slow')
    op.add_option('-t', '--timeout', dest='timeout',  type=float, default=150.0,
                  help='set test timeout in seconds')
    op.add_option('--no-progress', dest='hide_progress', action='store_true',
                  help='hide progress bar')
    op.add_option('--tinderbox', dest='tinderbox', action='store_true',
                  help='Tinderbox-parseable output format')
    op.add_option('--args', dest='shell_args', default='',
                  help='extra args to pass to the JS shell')
    op.add_option('-w', '--write-failures', dest='write_failures', metavar='FILE',
                  help='Write a list of failed tests to [FILE]')
    op.add_option('-r', '--read-tests', dest='read_tests', metavar='FILE',
                  help='Run test files listed in [FILE]')
    op.add_option('-R', '--retest', dest='retest', metavar='FILE',
                  help='Retest using test list file [FILE]')
    op.add_option('-g', '--debug', dest='debug', action='store_true',
                  help='Run test in gdb')
    op.add_option('--valgrind', dest='valgrind', action='store_true',
                  help='Enable the |valgrind| flag, if valgrind is in $PATH.')
    op.add_option('--valgrind-all', dest='valgrind_all', action='store_true',
                  help='Run all tests with valgrind, if valgrind is in $PATH.')
    op.add_option('--jitflags', dest='jitflags', default='',
                  help='Example: --jitflags=m,mn to run each test with "-m" and "-m -n" [default="%default"]. ' +
                       'Long flags, such as "--no-jm", should be set using --args.')
    op.add_option('--avoid-stdio', dest='avoid_stdio', action='store_true',
                  help='Use js-shell file indirection instead of piping stdio.')
    op.add_option('--write-failure-output', dest='write_failure_output', action='store_true',
                  help='With --write-failures=FILE, additionally write the output of failed tests to [FILE]')
    op.add_option('--ion', dest='ion', action='store_true',
                  help='Run tests once with --ion-eager and once with --no-jm (ignores --jitflags)')
    op.add_option('--tbpl', dest='tbpl', action='store_true',
                  help='Run tests with all IonMonkey option combinations (ignores --jitflags)')
    op.add_option('-j', '--worker-count', dest='max_jobs', type=int, default=max_jobs_default,
                  help='Number of tests to run in parallel (default %default)')

    (OPTIONS, args) = op.parse_args(argv)
    if len(args) < 1:
        op.error('missing JS_SHELL argument')
    # We need to make sure we are using backslashes on Windows.
    JS, test_args = os.path.normpath(args[0]), args[1:]

    if stdio_might_be_broken():
        # Prefer erring on the side of caution and not using stdio if
        # it might be broken on this platform.  The file-redirect
        # fallback should work on any platform, so at worst by
        # guessing wrong we might have slowed down the tests a bit.
        #
        # XXX technically we could check for broken stdio, but it
        # really seems like overkill.
        OPTIONS.avoid_stdio = True

    if OPTIONS.retest:
        OPTIONS.read_tests = OPTIONS.retest
        OPTIONS.write_failures = OPTIONS.retest

    test_list = []
    read_all = True

    if test_args:
        read_all = False
        for arg in test_args:
            test_list += find_tests(test_dir, arg)

    if OPTIONS.read_tests:
        read_all = False
        try:
            f = open(OPTIONS.read_tests)
            for line in f:
                test_list.append(os.path.join(test_dir, line.strip('\n')))
            f.close()
        except IOError:
            if OPTIONS.retest:
                read_all = True
            else:
                sys.stderr.write("Exception thrown trying to read test file '%s'\n"%
                                 OPTIONS.read_tests)
                traceback.print_exc()
                sys.stderr.write('---\n')

    if read_all:
        test_list = find_tests(test_dir)

    if OPTIONS.exclude:
        exclude_list = []
        for exclude in OPTIONS.exclude:
            exclude_list += find_tests(test_dir, exclude)
        test_list = [ test for test in test_list if test not in set(exclude_list) ]

    if not test_list:
        print >> sys.stderr, "No tests found matching command line arguments."
        sys.exit(0)

    test_list = [ Test.from_file(_, OPTIONS) for _ in test_list ]

    if not OPTIONS.run_slow:
        test_list = [ _ for _ in test_list if not _.slow ]

    # The full test list is ready. Now create copies for each JIT configuration.
    job_list = []
    if OPTIONS.tbpl:
        # Running all bits would take forever. Instead, we test a few interesting combinations.
        flags = [
                      ['--no-jm'],
                      ['--ion-eager'],
                      # Below, equivalents the old shell flags: ,m,am,amd,n,mn,amn,amdn,mdn
                      ['--no-ion', '--no-jm', '--no-ti'],
                      ['--no-ion', '--no-ti'],
                      ['--no-ion', '--no-ti', '-a', '-d'],
                      ['--no-ion', '--no-jm'],
                      ['--no-ion'],
                      ['--no-ion', '-a'],
                      ['--no-ion', '-a', '-d'],
                      ['--no-ion', '-d']
                    ]
        for test in test_list:
            for variant in flags:
                new_test = test.copy()
                new_test.jitflags.extend(variant)
                job_list.append(new_test)
    elif OPTIONS.ion:
        flags = [['--no-jm'], ['--ion-eager']]
        for test in test_list:
            for variant in flags:
                new_test = test.copy()
                new_test.jitflags.extend(variant)
                job_list.append(new_test)
    else:
        jitflags_list = parse_jitflags()
        for test in test_list:
            for jitflags in jitflags_list:
                new_test = test.copy()
                new_test.jitflags.extend(jitflags)
                job_list.append(new_test)

    shell_args = shlex.split(OPTIONS.shell_args)

    if OPTIONS.debug:
        if len(job_list) > 1:
            print('Multiple tests match command line arguments, debugger can only run one')
            for tc in job_list:
                print('    %s'%tc.path)
            sys.exit(1)

        tc = job_list[0]
        cmd = [ 'gdb', '--args' ] + get_test_cmd(tc.path, tc.jitflags, lib_dir, shell_args)
        call(cmd)
        sys.exit()

    try:
        ok = None
        if OPTIONS.max_jobs > 1 and HAVE_MULTIPROCESSING:
            ok = run_tests_parallel(job_list, test_dir, lib_dir, shell_args)
        else:
            ok = run_tests(job_list, test_dir, lib_dir, shell_args)
        if not ok:
            sys.exit(2)
    except OSError:
        if not os.path.exists(JS):
            print >> sys.stderr, "JS shell argument: file does not exist: '%s'"%JS
            sys.exit(1)
        else:
            raise

if __name__ == '__main__':
    main(sys.argv[1:])
