# trace-test.py -- Python harness for JavaScript trace tests.
#
# Requires Python 3.1

import datetime, os, re, sys
from subprocess import *

JS = None

def find_tests(path):
    if os.path.isfile(path):
        if path.endswith('.js'):
            return [ path ]
        else:
            print('Not a javascript file: {0}'.format(path), file=sys.stderr)
            sys.exit(1)

    if os.path.isdir(path):
        return find_tests_dir(path)

    print('Not a file or directory: {0}'.format(path), file=sys.stderr)
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
            test = os.path.relpath(os.path.join(dirpath, filename))
            ans.append(test)
    return ans

def get_test_cmd(path, lib_dir):
    libdir_var = lib_dir
    if not libdir_var.endswith('/'):
        libdir_var += '/'
    expr = 'const platform="{}"; const libdir="{}";'.format(sys.platform, libdir_var)
    cmd = '{JS} -j -e \'{expr}\' -f {prolog} -f {path}'.format(
        JS=JS, path=path, expr=expr,
        prolog=os.path.join(lib_dir, 'prolog.js'))
    return cmd

def run_test(path, lib_dir):
    cmd = get_test_cmd(path, lib_dir)
    if OPTIONS.show_cmd:
        print(cmd)
    p = Popen(cmd, shell=True, stdin=PIPE, stdout=PIPE, stderr=PIPE, close_fds=True)
    out, err = p.communicate()
    out, err = out.decode(), err.decode()
    if OPTIONS.show_output:
        sys.stdout.write(out)
        sys.stdout.write(err)
    return check_output(out, err, p.returncode)

assert_re = re.compile(r'Assertion failed:')
stat_re = re.compile(r'^Trace stats check failed')

def check_output(out, err, rc):
    if rc != 0:
        return False

    for line in out.split('\n'):
        if stat_re.match(line):
            return False

    for line in err.split('\n'):
        if assert_re.match(line):
            return False

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
            if not run_test(test, lib_dir):
                failures.append(test)

            n = i + 1
            if pb:
                pb.label = '[{:3}|{:3}|{:3}]'.format(n - len(failures), len(failures), n)
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
                print('    {}'.format(get_test_cmd(test, lib_dir)))
            else:
                print('    {}'.format(test))
    else:
        print('PASSED ALL' + ('' if complete else ' (partial run -- interrupted by user)'))

if __name__ == '__main__':
    script_path = os.path.abspath(__file__)
    script_dir = os.path.dirname(script_path)
    test_dir = os.path.relpath(os.path.join(script_dir, 'tests'))
    lib_dir = os.path.relpath(os.path.join(script_dir, 'lib'))

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
    (OPTIONS, args) = op.parse_args()
    if len(args) < 1:
        op.error('missing JS_SHELL argument')
    JS, *test_args = args

    if test_args:
        test_list = []
        for arg in test_args:
            test_list += find_tests(os.path.join(test_dir, arg))
    else:
        test_list = find_tests(test_dir)


    if OPTIONS.exclude:
        exclude_list = []
        for exclude in OPTIONS.exclude:
            exclude_list += find_tests(os.path.join(test_dir, exclude))
        test_list = [ test for test in test_list if test not in set(exclude_list) ]

    if not test_list:
        print("No tests found matching command line arguments.", file=sys.stderr)
        sys.exit(0)
        
    run_tests(test_list, lib_dir)
