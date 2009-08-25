# trace-test.py -- Python harness for JavaScript trace tests.

import datetime, os, re, sys
import subprocess
from subprocess import *

JS = None

def find_tests(path):
    if os.path.isfile(path):
        if path.endswith('.js'):
            return [ path ]
        else:
            print >> sys.stderr, 'Not a javascript file: %s'%path
            sys.exit(1)

    if os.path.isdir(path):
        return find_tests_dir(path)

    print >> sys.stderr, 'Not a file or directory: %s'%path
    sys.exit(1)

def find_tests_dir(dir):
    ans = []
    for dirpath, dirnames, filenames in os.walk(dir):
        if dirpath == '.':
            continue
        for filename in filenames:
            if not filename.endswith('.js'):
                continue
            if filename in ('shell.js', 'browser.js', 'jsref.js'):
                continue
            test = os.path.join(dirpath, filename)
            ans.append(test)
    return ans

def get_test_cmd(path, lib_dir):
    libdir_var = lib_dir
    if not libdir_var.endswith('/'):
        libdir_var += '/'
    expr = "const platform=%r; const libdir=%r;"%(sys.platform, libdir_var)
    return [ JS, '-j', '-e', expr, '-f', os.path.join(lib_dir, 'prolog.js'),
             '-f', path ]

def run_test(path, lib_dir):
    cmd = get_test_cmd(path, lib_dir)
    if OPTIONS.show_cmd:
        print(cmd)
    # close_fds is not supported on Windows and will cause a ValueError.
    close_fds = sys.platform != 'win32'
    p = Popen(cmd, stdin=PIPE, stdout=PIPE, stderr=PIPE, close_fds=close_fds)
    out, err = p.communicate()
    out, err = out.decode(), err.decode()
    if OPTIONS.show_output:
        sys.stdout.write(out)
        sys.stdout.write(err)
    # Determine whether or not we can allow an out-of-memory condition.
    allow_oom = 'allow_oom' in path
    return (check_output(out, err, p.returncode, allow_oom), out, err)

def check_output(out, err, rc, allow_oom):
    for line in out.split('\n'):
        if line.startswith('Trace stats check failed'):
            return False

    for line in err.split('\n'):
        if 'Assertion failed:' in line:
            return False

    if rc != 0:
        # Allow a non-zero exit code if we want to allow OOM, but only if we
        # actually got OOM.
        return allow_oom and ': out of memory\n' in err

    return True

def run_tests(tests, lib_dir):
    pb = None
    if not OPTIONS.hide_progress and not OPTIONS.show_cmd:
        try:
            from progressbar import ProgressBar
            pb = ProgressBar('', len(tests), 13)
        except ImportError:
            pass

    failures = []
    complete = False
    try:
        for i, test in enumerate(tests):
            ok, out, err = run_test(test, lib_dir)
            if not ok:
                failures.append(test)

            if OPTIONS.tinderbox:
                if ok:
                    print('TEST-PASS | trace-test.py | %s'%test)
                else:
                    lines = [ _ for _ in out.split('\n') + err.split('\n')
                              if _ != '' ]
                    if len(lines) >= 1:
                        msg = lines[-1]
                    else:
                        msg = ''
                    print('TEST-UNEXPECTED-FAIL | trace-test.py | %s: %s'%
                          (test, msg))

            n = i + 1
            if pb:
                pb.label = '[%3d|%3d|%3d]'%(n - len(failures), len(failures), n)
                pb.update(n)
        complete = True
    except KeyboardInterrupt:
        pass

    if pb: 
        pb.finish()

    if failures:
        print('FAILURES:')
        for test in failures:
            if OPTIONS.show_failed:
                print('    ' + subprocess.list2cmdline(get_test_cmd(test, lib_dir)))
            else:
                print('    ' + test)
    else:
        print('PASSED ALL' + ('' if complete else ' (partial run -- interrupted by user)'))

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
    op.add_option('--no-progress', dest='hide_progress', action='store_true',
                  help='hide progress bar')
    op.add_option('--tinderbox', dest='tinderbox', action='store_true',
                  help='Tinderbox-parseable output format')
    (OPTIONS, args) = op.parse_args()
    if len(args) < 1:
        op.error('missing JS_SHELL argument')
    # We need to make sure we are using backslashes on Windows.
    JS, test_args = os.path.normpath(args[0]), args[1:]

    if test_args:
        test_list = []
        for arg in test_args:
            test_list += find_tests(os.path.normpath(os.path.join(test_dir, arg)))
    else:
        test_list = find_tests(test_dir)

    if OPTIONS.exclude:
        exclude_list = []
        for exclude in OPTIONS.exclude:
            exclude_list += find_tests(os.path.normpath(os.path.join(test_dir, exclude)))
        test_list = [ test for test in test_list if test not in set(exclude_list) ]

    if not test_list:
        print >> sys.stderr, "No tests found matching command line arguments."
        sys.exit(0)
        
    run_tests(test_list, lib_dir)
