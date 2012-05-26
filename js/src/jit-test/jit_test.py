#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# jit_test.py -- Python harness for JavaScript trace tests.

import datetime, os, re, sys, tempfile, traceback, time, shlex
import subprocess
from subprocess import *
from threading import Thread

DEBUGGER_INFO = {
  "gdb": {
    "interactive": True,
    "args": "-q --args"
  },

  "valgrind": {
    "interactive": False,
    "args": "--leak-check=full"
  }
}

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
        self.expect_error = '' # Errors to expect and consider passing
        self.expect_status = 0 # Exit status to expect from shell

    def copy(self):
        t = Test(self.path)
        t.jitflags = self.jitflags[:]
        t.slow = self.slow
        t.allow_oom = self.allow_oom
        t.valgrind = self.valgrind
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
                    elif name == 'mjitalways':
                        test.jitflags.append('-a')
                    elif name == 'debug':
                        test.jitflags.append('-d')
                    elif name == 'mjit':
                        test.jitflags.append('-m')
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
                            '--smc-check=all',
                            '--error-exitcode=1',
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
    out, err, code, timed_out = run(cmd, os.environ, OPTIONS.timeout)

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
        return test.allow_oom and ': out of memory' in err and 'Assertion failure' not in err

    return True

def print_tinderbox(label, test, message=None):
    jitflags = " ".join(test.jitflags)
    result = "%s | jit_test.py %-15s| %s" % (label, jitflags, test.path)
    if message:
        result += ": " + message
    print result

def run_tests(tests, test_dir, lib_dir, shell_args):
    pb = None
    if not OPTIONS.hide_progress and not OPTIONS.show_cmd:
        try:
            from progressbar import ProgressBar
            pb = ProgressBar('', len(tests), 24)
        except ImportError:
            pass

    failures = []
    timeouts = 0
    complete = False
    doing = 'before starting'
    try:
        for i, test in enumerate(tests):
            doing = 'on %s'%test.path
            ok, out, err, code, timed_out = run_test(test, lib_dir, shell_args)
            doing = 'after %s'%test.path

            if not ok:
                failures.append([ test, out, err, code, timed_out ])
            if timed_out:
                timeouts += 1

            if OPTIONS.tinderbox:
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
            if pb:
                pb.label = '[%4d|%4d|%4d|%4d]'%(n - len(failures), len(failures), timeouts, n)
                pb.update(n)
        complete = True
    except KeyboardInterrupt:
        print_tinderbox("TEST-UNEXPECTED-FAIL", test);

    if pb:
        pb.finish()

    if failures:
        if OPTIONS.write_failures:
            try:
                out = open(OPTIONS.write_failures, 'w')
                # Don't write duplicate entries when we are doing multiple failures per job.
                written = set()
                for test, fout, ferr, fcode, _ in failures:
                    if test.path not in written:
                        out.write(os.path.relpath(test.path, test_dir) + '\n')
                        if OPTIONS.write_failure_output:
                            out.write(fout)
                            out.write(ferr)
                            out.write('Exit code: ' + str(fcode) + "\n")
                        written.add(test.path)
                out.close()
            except IOError:
                sys.stderr.write("Exception thrown trying to write failure file '%s'\n"%
                                 OPTIONS.write_failures)
                traceback.print_exc()
                sys.stderr.write('---\n')

        def show_test(test):
            if OPTIONS.show_failed:
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
    op.add_option('--jitflags', dest='jitflags', default='m,mn',
                  help='Example: --jitflags=m,mn to run each test with -m, -m -n [default=%default]')
    op.add_option('--avoid-stdio', dest='avoid_stdio', action='store_true',
                  help='Use js-shell file indirection instead of piping stdio.')
    op.add_option('--write-failure-output', dest='write_failure_output', action='store_true',
                  help='With --write-failures=FILE, additionally write the output of failed tests to [FILE]')
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
