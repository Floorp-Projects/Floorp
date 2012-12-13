#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# run-tests.py -- Python harness for GDB SpiderMonkey support

import os, re, subprocess, sys, traceback
from threading import Thread

# From this directory:
import progressbar
from taskpool import TaskPool, get_cpu_count

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

# Characters that need to be escaped when used in shell words.
shell_need_escapes = re.compile('[^\w\d%+,-./:=@\'"]', re.DOTALL)
# Characters that need to be escaped within double-quoted strings.
shell_dquote_escapes = re.compile('[^\w\d%+,-./:=@"]', re.DOTALL)
def make_shell_cmd(l):
    def quote(s):
        if shell_need_escapes.search(s):
            if s.find("'") < 0:
                return "'" + s + "'"
            return '"' + shell_dquote_escapes.sub('\\g<0>', s) + '"'
        return s

    return ' '.join([quote(_) for _ in l])

# An instance of this class collects the lists of passing, failing, and
# timing-out tests, runs the progress bar, and prints a summary at the end.
class Summary(object):

    class SummaryBar(progressbar.ProgressBar):
        def __init__(self, limit):
            super(Summary.SummaryBar, self).__init__('', limit, 24)
        def start(self):
            self.label = '[starting           ]'
            self.update(0)
        def counts(self, run, failures, timeouts):
            self.label = '[%4d|%4d|%4d|%4d]' % (run - failures, failures, timeouts, run)
            self.update(run)

    def __init__(self, num_tests):
        self.run = 0
        self.failures = []              # kind of judgemental; "unexpecteds"?
        self.timeouts = []
        if not OPTIONS.hide_progress:
            self.bar = Summary.SummaryBar(num_tests)

    # Progress bar control.
    def start(self):
        if not OPTIONS.hide_progress:
            self.bar.start()
    def update(self):
        if not OPTIONS.hide_progress:
            self.bar.counts(self.run, len(self.failures), len(self.timeouts))
    # Call 'thunk' to show some output, while getting the progress bar out of the way.
    def interleave_output(self, thunk):
        if not OPTIONS.hide_progress:
            self.bar.clear()
        thunk()
        self.update()

    def passed(self, test):
        self.run += 1
        self.update()

    def failed(self, test):
        self.run += 1
        self.failures.append(test)
        self.update()

    def timeout(self, test):
        self.run += 1
        self.timeouts.append(test)
        self.update()

    def finish(self):
        if not OPTIONS.hide_progress:
            self.bar.finish()

        if self.failures:

            print "tests failed:"
            for test in self.failures:
                test.show(sys.stdout)

            if OPTIONS.worklist:
                try:
                    with open(OPTIONS.worklist) as out:
                        for test in self.failures:
                            out.write(test.name + '\n')
                except IOError as err:
                    sys.stderr.write("Error writing worklist file '%s': %s"
                                     % (OPTIONS.worklist, err))
                    sys.exit(1)

            if OPTIONS.write_failures:
                try:
                    with open(OPTIONS.write_failures) as out:
                        for test in self.failures:
                            test.show(out)
                except IOError as err:
                    sys.stderr.write("Error writing worklist file '%s': %s"
                                     % (OPTIONS.write_failures, err))
                    sys.exit(1)

        if self.timeouts:
            print "tests timed out:"
            for test in self.timeouts:
                test.show(sys.stdout)

        if self.failures or self.timeouts:
            sys.exit(2)

class Test(TaskPool.Task):
    def __init__(self, path, summary):
        super(Test, self).__init__()
        self.test_path = path           # path to .py test file
        self.summary = summary

        # test.name is the name of the test relative to the top of the test
        # directory. This is what we use to report failures and timeouts,
        # and when writing test lists.
        self.name = os.path.relpath(self.test_path, OPTIONS.testdir)

        self.stdout = ''
        self.stderr = ''
        self.returncode = None

    def cmd(self):
        testlibdir = os.path.normpath(os.path.join(OPTIONS.testdir, '..', 'lib-for-tests'))
        return [OPTIONS.gdb_executable,
                '-nw',          # Don't create a window (unnecessary?)
                '-nx',          # Don't read .gdbinit.
                '--ex', 'add-auto-load-safe-path %s' % (OPTIONS.builddir,),
                '--ex', 'set env LD_LIBRARY_PATH %s' % (OPTIONS.libdir,),
                '--ex', 'file %s' % (os.path.join(OPTIONS.builddir, 'gdb-tests'),),
                '--eval-command', 'python testlibdir=%r' % (testlibdir,),
                '--eval-command', 'python testscript=%r' % (self.test_path,),
                '--eval-command', 'python execfile(%r)' % os.path.join(testlibdir, 'catcher.py')]

    def start(self, pipe, deadline):
        super(Test, self).start(pipe, deadline)
        if OPTIONS.show_cmd:
            self.summary.interleave_output(lambda: self.show_cmd(sys.stdout))

    def onStdout(self, text):
        self.stdout += text

    def onStderr(self, text):
        self.stderr += text

    def onFinished(self, returncode):
        self.returncode = returncode
        if OPTIONS.show_output:
            self.summary.interleave_output(lambda: self.show_output(sys.stdout))
        if returncode != 0:
            self.summary.failed(self)
        else:
            self.summary.passed(self)

    def onTimeout(self):
        self.summary.timeout(self)

    def show_cmd(self, out):
        print "Command: ", make_shell_cmd(self.cmd())

    def show_output(self, out):
        if self.stdout:
            out.write('Standard output:')
            out.write('\n' + self.stdout + '\n')
        if self.stderr:
            out.write('Standard error:')
            out.write('\n' + self.stderr + '\n')

    def show(self, out):
        out.write(self.name + '\n')
        if OPTIONS.write_failure_output:
            out.write('Command: %s\n' % (make_shell_cmd(self.cmd()),))
            self.show_output(out)
            out.write('GDB exit code: %r\n' % (self.returncode,))

def find_tests(dir, substring = None):
    ans = []
    for dirpath, dirnames, filenames in os.walk(dir):
        if dirpath == '.':
            continue
        for filename in filenames:
            if not filename.endswith('.py'):
                continue
            test = os.path.join(dirpath, filename)
            if substring is None or substring in os.path.relpath(test, dir):
                ans.append(test)
    return ans

def build_test_exec(builddir):
    p = subprocess.check_call(['make', 'gdb-tests'], cwd=builddir)

def run_tests(tests, summary):
    pool = TaskPool(tests, job_limit=OPTIONS.workercount, timeout=OPTIONS.timeout)
    pool.run_all()

OPTIONS = None
def main(argv):
    global OPTIONS
    script_path = os.path.abspath(__file__)
    script_dir = os.path.dirname(script_path)

    # LIBDIR is the directory in which we find the SpiderMonkey shared
    # library, to link against.
    #
    # The [TESTS] optional arguments are paths of test files relative
    # to the jit-test/tests directory.
    from optparse import OptionParser
    op = OptionParser(usage='%prog [options] LIBDIR [TESTS...]')
    op.add_option('-s', '--show-cmd', dest='show_cmd', action='store_true',
                  help='show GDB shell command run')
    op.add_option('-o', '--show-output', dest='show_output', action='store_true',
                  help='show output from GDB')
    op.add_option('-x', '--exclude', dest='exclude', action='append',
                  help='exclude given test dir or path')
    op.add_option('-t', '--timeout', dest='timeout',  type=float, default=150.0,
                  help='set test timeout in seconds')
    op.add_option('-j', '--worker-count', dest='workercount', type=int,
                  help='Run [WORKERCOUNT] tests at a time')
    op.add_option('--no-progress', dest='hide_progress', action='store_true',
                  help='hide progress bar')
    op.add_option('--worklist', dest='worklist', metavar='FILE',
                  help='Read tests to run from [FILE] (or run all if [FILE] not found);\n'
                       'write failures back to [FILE]')
    op.add_option('-r', '--read-tests', dest='read_tests', metavar='FILE',
                  help='Run test files listed in [FILE]')
    op.add_option('-w', '--write-failures', dest='write_failures', metavar='FILE',
                  help='Write failing tests to [FILE]')
    op.add_option('--write-failure-output', dest='write_failure_output', action='store_true',
                  help='With --write-failures=FILE, additionally write the output of failed tests to [FILE]')
    op.add_option('--gdb', dest='gdb_executable', metavar='EXECUTABLE', default='gdb',
                  help='Run tests with [EXECUTABLE], rather than plain \'gdb\'.')
    op.add_option('--srcdir', dest='srcdir',
                  default=os.path.abspath(os.path.join(script_dir, '..')),
                  help='Use SpiderMonkey sources in [SRCDIR].')
    op.add_option('--testdir', dest='testdir', default=os.path.join(script_dir, 'tests'),
                  help='Find tests in [TESTDIR].')
    op.add_option('--builddir', dest='builddir',
                  help='Build test executable in [BUILDDIR].')
    (OPTIONS, args) = op.parse_args(argv)
    if len(args) < 1:
        op.error('missing LIBDIR argument')
    OPTIONS.libdir = os.path.abspath(args[0])
    test_args = args[1:]

    if not OPTIONS.workercount:
        OPTIONS.workercount = get_cpu_count()

    # Compute default for OPTIONS.builddir now, since we've computed OPTIONS.libdir.
    if not OPTIONS.builddir:
        OPTIONS.builddir = os.path.join(OPTIONS.libdir, 'gdb')

    test_set = set()

    # All the various sources of test names accumulate.
    if test_args:
        for arg in test_args:
            test_set.update(find_tests(OPTIONS.testdir, arg))
    if OPTIONS.worklist:
        try:
            with open(OPTIONS.worklist) as f:
                for line in f:
                    test_set.update(os.path.join(test_dir, line.strip('\n')))
        except IOError:
            # With worklist, a missing file means to start the process with
            # the complete list of tests.
            sys.stderr.write("Couldn't read worklist file '%s'; running all tests\n"
                             % (OPTIONS.worklist,))
            test_set = set(find_tests(OPTIONS.testdir))
    if OPTIONS.read_tests:
        try:
            with open(OPTIONS.read_tests) as f:
                for line in f:
                    test_set.update(os.path.join(test_dir, line.strip('\n')))
        except IOError as err:
            sys.stderr.write("Error trying to read test file '%s': %s\n"
                             % (OPTIONS.read_tests, err))
            sys.exit(1)

    # If none of the above options were passed, and no tests were listed
    # explicitly, use the complete set.
    if not test_args and not OPTIONS.worklist and not OPTIONS.read_tests:
        test_set = set(find_tests(OPTIONS.testdir))

    if OPTIONS.exclude:
        exclude_set = set()
        for exclude in OPTIONS.exclude:
            exclude_set.update(find_tests(test_dir, exclude))
        test_set -= exclude_set

    if not test_set:
        sys.stderr.write("No tests found matching command line arguments.\n")
        sys.exit(1)

    summary = Summary(len(test_set))
    test_list = [ Test(_, summary) for _ in sorted(test_set) ]

    # Build the test executable from all the .cpp files found in the test
    # directory tree.
    try:
        build_test_exec(OPTIONS.builddir)
    except subprocess.CalledProcessError as err:
        sys.stderr.write("Error building test executable: %s\n" % (err,))
        sys.exit(1)

    # Run the tests.
    try:
        summary.start()
        run_tests(test_list, summary)
        summary.finish()
    except OSError as err:
        sys.stderr.write("Error running tests: %s\n", (err,))
        sys.exit(1)

    sys.exit(0)

if __name__ == '__main__':
    main(sys.argv[1:])
