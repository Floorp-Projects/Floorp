# trace-test.py -- Python harness for JavaScript trace tests.

import datetime, os, re, sys, traceback
import subprocess
from subprocess import *

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
    def __init__(self, path, slow, allow_oom, tmflags, error, valgrind):
        """  path        path to test file
             slow        True means the test is slow-running
             allow_oom   True means OOM should not be considered a failure
             valgrind    True means test should run under valgrind """
        self.path = path
        self.slow = slow
        self.allow_oom = allow_oom
        self.tmflags = tmflags
        self.error = error
        self.valgrind = valgrind

    COOKIE = '|trace-test|'

    @classmethod
    def from_file(cls, path, options):
        slow = allow_oom = valgrind = False
        error = tmflags = ''

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
                    if name == 'TMFLAGS':
                        tmflags = value
                    elif name == 'error':
                        error = value
                    else:
                        print('warning: unrecognized |trace-test| attribute %s'%part)
                else:
                    if name == 'slow':
                        slow = True
                    elif name == 'allow-oom':
                        allow_oom = True
                    elif name == 'valgrind':
                        valgrind = options.valgrind
                    else:
                        print('warning: unrecognized |trace-test| attribute %s'%part)

        return cls(path, slow, allow_oom, tmflags, error, valgrind or options.valgrind_all)

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

def get_test_cmd(path, lib_dir):
    libdir_var = lib_dir
    if not libdir_var.endswith('/'):
        libdir_var += '/'
    expr = "const platform=%r; const libdir=%r;"%(sys.platform, libdir_var)
    return [ JS, '-j', '-e', expr, '-f', os.path.join(lib_dir, 'prolog.js'),
             '-f', path ]

def run_test(test, lib_dir):
    if test.tmflags:
        env = os.environ.copy()
        env['TMFLAGS'] = test.tmflags
    else:
        env = None
    cmd = get_test_cmd(test.path, lib_dir)

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
        print(cmd)
    # close_fds is not supported on Windows and will cause a ValueError.
    close_fds = sys.platform != 'win32'
    p = Popen(cmd, stdin=PIPE, stdout=PIPE, stderr=PIPE, close_fds=close_fds, env=env)
    out, err = p.communicate()
    out, err = out.decode(), err.decode()
    if OPTIONS.show_output:
        sys.stdout.write(out)
        sys.stdout.write(err)
    if test.valgrind:
        sys.stdout.write(err)
    return (check_output(out, err, p.returncode, test.allow_oom, test.error), 
            out, err)

def check_output(out, err, rc, allow_oom, expectedError):
    if expectedError:
        return expectedError in err

    for line in out.split('\n'):
        if line.startswith('Trace stats check failed'):
            return False

    for line in err.split('\n'):
        if 'Assertion failed:' in line:
            return False

    if rc != 0:
        # Allow a non-zero exit code if we want to allow OOM, but only if we
        # actually got OOM.
        return allow_oom and ': out of memory' in err

    return True

def run_tests(tests, test_dir, lib_dir):
    pb = None
    if not OPTIONS.hide_progress and not OPTIONS.show_cmd:
        try:
            from progressbar import ProgressBar
            pb = ProgressBar('', len(tests), 13)
        except ImportError:
            pass

    failures = []
    complete = False
    doing = 'before starting'
    try:
        for i, test in enumerate(tests):
            doing = 'on %s'%test.path
            ok, out, err = run_test(test, lib_dir)
            doing = 'after %s'%test.path

            if not ok:
                failures.append(test.path)

            if OPTIONS.tinderbox:
                if ok:
                    print('TEST-PASS | trace-test.py | %s'%test.path)
                else:
                    lines = [ _ for _ in out.split('\n') + err.split('\n')
                              if _ != '' ]
                    if len(lines) >= 1:
                        msg = lines[-1]
                    else:
                        msg = ''
                    print('TEST-UNEXPECTED-FAIL | trace-test.py | %s: %s'%
                          (test.path, msg))

            n = i + 1
            if pb:
                pb.label = '[%3d|%3d|%3d]'%(n - len(failures), len(failures), n)
                pb.update(n)
        complete = True
    except KeyboardInterrupt:
        print('TEST-UNEXPECTED_FAIL | trace-test.py | %s'%test.path)

    if pb:
        pb.finish()

    if failures:
        if OPTIONS.write_failures:
            try:
                out = open(OPTIONS.write_failures, 'w')
                for test in failures:
                    out.write(os.path.relpath(test, test_dir) + '\n')
                out.close()
            except IOError:
                sys.stderr.write("Exception thrown trying to write failure file '%s'\n"%
                                 OPTIONS.write_failures)
                traceback.print_exc()
                sys.stderr.write('---\n')

        print('FAILURES:')
        for test in failures:
            if OPTIONS.show_failed:
                print('    ' + subprocess.list2cmdline(get_test_cmd(test, lib_dir)))
            else:
                print('    ' + test)
        return False
    else:
        print('PASSED ALL' + ('' if complete else ' (partial run -- interrupted by user %s)'%doing))
        return True

if __name__ == '__main__':
    script_path = os.path.abspath(__file__)
    script_dir = os.path.dirname(script_path)
    test_dir = os.path.join(script_dir, 'tests')
    lib_dir = os.path.join(script_dir, 'lib')

    # The [TESTS] optional arguments are paths of test files relative
    # to the trace-test/tests directory.

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
    op.add_option('--no-progress', dest='hide_progress', action='store_true',
                  help='hide progress bar')
    op.add_option('--tinderbox', dest='tinderbox', action='store_true',
                  help='Tinderbox-parseable output format')
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
    (OPTIONS, args) = op.parse_args()
    if len(args) < 1:
        op.error('missing JS_SHELL argument')
    # We need to make sure we are using backslashes on Windows.
    JS, test_args = os.path.normpath(args[0]), args[1:]

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

    if OPTIONS.debug:
        if len(test_list) > 1:
            print('Multiple tests match command line arguments, debugger can only run one')
            for tc in test_list:
                print('    %s'%tc.path)
            sys.exit(1)

        tc = test_list[0]
        cmd = [ 'gdb', '--args' ] + get_test_cmd(tc.path, lib_dir)
        call(cmd)
        sys.exit()

    try:
        ok = run_tests(test_list, test_dir, lib_dir)
        if not ok:
            sys.exit(2)
    except OSError:
        if not os.path.exists(JS):
            print >> sys.stderr, "JS shell argument: file does not exist: '%s'"%JS
            sys.exit(1)
        else:
            raise
