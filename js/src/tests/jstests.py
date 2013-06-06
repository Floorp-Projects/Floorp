#!/usr/bin/env python
"""
The JS Shell Test Harness.

See the adjacent README.txt for more details.
"""

import os, sys, textwrap
from os.path import abspath, dirname, realpath
from copy import copy
from subprocess import list2cmdline, call

from lib.results import NullTestOutput
from lib.tests import TestCase
from lib.results import ResultsSink
from lib.progressbar import ProgressBar

if (sys.platform.startswith('linux') or
    sys.platform.startswith('darwin')
   ):
    from lib.tasks_unix import run_all_tests
else:
    from lib.tasks_win import run_all_tests

def run_tests(options, tests, results):
    """Run the given tests, sending raw results to the given results accumulator."""
    try:
        completed = run_all_tests(tests, results, options)
    except KeyboardInterrupt:
        completed = False

    results.finish(completed)

def get_cpu_count():
    """
    Guess at a reasonable parallelism count to set as the default for the
    current machine and run.
    """
    # Python 2.6+
    try:
        import multiprocessing
        return multiprocessing.cpu_count()
    except (ImportError,NotImplementedError):
        pass

    # POSIX
    try:
        res = int(os.sysconf('SC_NPROCESSORS_ONLN'))
        if res > 0:
            return res
    except (AttributeError,ValueError):
        pass

    # Windows
    try:
        res = int(os.environ['NUMBER_OF_PROCESSORS'])
        if res > 0:
            return res
    except (KeyError, ValueError):
        pass

    return 1

def parse_args():
    """
    Parse command line arguments.
    Returns a tuple of: (options, js_shell, requested_paths, excluded_paths)
        options :object: The raw OptionParser output.
        js_shell :str: The absolute location of the shell to test with.
        requested_paths :set<str>: Test paths specially requested on the CLI.
        excluded_paths :set<str>: Test paths specifically excluded by the CLI.
    """
    from optparse import OptionParser, OptionGroup
    op = OptionParser(usage=textwrap.dedent("""
        %prog [OPTIONS] JS_SHELL [TESTS]

        Shell output format: [ pass | fail | timeout | skip ] progress | time
        """).strip())
    op.add_option('--xul-info', dest='xul_info_src',
                  help='config data for xulRuntime (avoids search for config/autoconf.mk)')

    harness_og = OptionGroup(op, "Harness Controls", "Control how tests are run.")
    harness_og.add_option('-j', '--worker-count', type=int, default=max(1, get_cpu_count()),
                          help='Number of tests to run in parallel (default %default)')
    harness_og.add_option('-t', '--timeout', type=float, default=150.0,
                          help='Set maximum time a test is allows to run (in seconds).')
    harness_og.add_option('-a', '--args', dest='shell_args', default='',
                          help='Extra args to pass to the JS shell.')
    harness_og.add_option('--jitflags', default='',
                          help='Example: --jitflags=m,amd to run each test with -m, -a -m -d [default=%default]')
    harness_og.add_option('-g', '--debug', action='store_true', help='Run a test in debugger.')
    harness_og.add_option('--debugger', default='gdb -q --args', help='Debugger command.')
    harness_og.add_option('-J', '--jorendb', action='store_true', help='Run under JS debugger.')
    harness_og.add_option('--passthrough', action='store_true', help='Run tests with stdin/stdout attached to caller.')
    harness_og.add_option('--valgrind', action='store_true', help='Run tests in valgrind.')
    harness_og.add_option('--valgrind-args', default='', help='Extra args to pass to valgrind.')
    op.add_option_group(harness_og)

    input_og = OptionGroup(op, "Inputs", "Change what tests are run.")
    input_og.add_option('-f', '--file', dest='test_file', action='append',
                        help='Get tests from the given file.')
    input_og.add_option('-x', '--exclude-file', action='append',
                        help='Exclude tests from the given file.')
    input_og.add_option('-d', '--exclude-random', dest='random', action='store_false',
                        help='Exclude tests marked as "random."')
    input_og.add_option('--run-skipped', action='store_true', help='Run tests marked as "skip."')
    input_og.add_option('--run-only-skipped', action='store_true', help='Run only tests marked as "skip."')
    input_og.add_option('--run-slow-tests', action='store_true',
                        help='Do not skip tests marked as "slow."')
    input_og.add_option('--no-extensions', action='store_true',
                        help='Run only tests conforming to the ECMAScript 5 standard.')
    op.add_option_group(input_og)

    output_og = OptionGroup(op, "Output", "Modify the harness and tests output.")
    output_og.add_option('-s', '--show-cmd', action='store_true',
                         help='Show exact commandline used to run each test.')
    output_og.add_option('-o', '--show-output', action='store_true',
                         help="Print each test's output to the file given by --output-file.")
    output_og.add_option('-F', '--failed-only', action='store_true',
                         help="If a --show-* option is given, only print output for failed tests.")
    output_og.add_option('-O', '--output-file',
                         help='Write all output to the given file (default: stdout).')
    output_og.add_option('--failure-file',
                         help='Write all not-passed tests to the given file.')
    output_og.add_option('--no-progress', dest='hide_progress', action='store_true',
                         help='Do not show the progress bar.')
    output_og.add_option('--tinderbox', action='store_true',
                         help='Use tinderbox-parseable output format.')
    op.add_option_group(output_og)

    special_og = OptionGroup(op, "Special", "Special modes that do not run tests.")
    special_og.add_option('--make-manifests', metavar='BASE_TEST_PATH',
                          help='Generate reftest manifest files.')
    op.add_option_group(special_og)
    options, args = op.parse_args()

    # Acquire the JS shell given on the command line.
    options.js_shell = None
    requested_paths = set()
    if len(args) > 0:
        options.js_shell = abspath(args[0])
        requested_paths |= set(args[1:])

    # If we do not have a shell, we must be in a special mode.
    if options.js_shell is None and not options.make_manifests:
        op.error('missing JS_SHELL argument')

    # Valgrind and gdb are mutually exclusive.
    if options.valgrind and options.debug:
        op.error("--valgrind and --debug are mutually exclusive.")

    # Fill the debugger field, as needed.
    prefix = options.debugger.split() if options.debug else []
    if options.valgrind:
        prefix = ['valgrind'] + options.valgrind_args.split()
        if os.uname()[0] == 'Darwin':
            prefix.append('--dsymutil=yes')
        options.show_output = True

    js_cmd_args = options.shell_args.split()
    if options.jorendb:
        options.passthrough = True
        options.hide_progress = True
        options.worker_count = 1
        debugger_path = realpath(os.path.join(abspath(dirname(abspath(__file__))), '..', '..', 'examples', 'jorendb.js'))
        js_cmd_args.extend([ '-d', '-f', debugger_path, '--' ])
    TestCase.set_js_cmd_prefix(options.js_shell, js_cmd_args, prefix)

    # If files with lists of tests to run were specified, add them to the
    # requested tests set.
    if options.test_file:
        for test_file in options.test_file:
            requested_paths |= set([line.strip() for line in open(test_file).readlines()])

    # If files with lists of tests to exclude were specified, add them to the
    # excluded tests set.
    excluded_paths = set()
    if options.exclude_file:
        for filename in options.exclude_file:
            try:
                fp = open(filename, 'r')
                for line in fp:
                    if line.startswith('#'): continue
                    line = line.strip()
                    if not line: continue
                    excluded_paths |= set((line,))
            finally:
                fp.close()

    # Handle output redirection, if requested and relevant.
    options.output_fp = sys.stdout
    if options.output_file:
        if not options.show_cmd:
            options.show_output = True
        try:
            options.output_fp = open(options.output_file, 'w')
        except IOError, ex:
            raise SystemExit("Failed to open output file: " + str(ex))

    options.show = options.show_cmd or options.show_output

    # Hide the progress bar if it will get in the way of other output.
    options.hide_progress = (options.tinderbox or
                             not ProgressBar.conservative_isatty() or
                             options.hide_progress)

    return (options, requested_paths, excluded_paths)

def parse_jitflags(op_jitflags):
    jitflags = [ [ '-' + flag for flag in flags ]
                 for flags in op_jitflags.split(',') ]
    for flags in jitflags:
        for flag in flags:
            if flag not in ('-m', '-a', '-p', '-d', '-n'):
                print('Invalid jit flag: "%s"'%flag)
                sys.exit(1)
    return jitflags

def load_tests(options, requested_paths, excluded_paths):
    """
    Returns a tuple: (skipped_tests, test_list)
        skip_list: [iterable<Test>] Tests found but skipped.
        test_list: [iterable<Test>] Tests found that should be run.
    """
    import lib.manifest as manifest

    if options.js_shell is None:
        xul_tester = manifest.NullXULInfoTester()
    else:
        if options.xul_info_src is None:
            xul_info = manifest.XULInfo.create(options.js_shell)
        else:
            xul_abi, xul_os, xul_debug = options.xul_info_src.split(r':')
            xul_debug = xul_debug.lower() is 'true'
            xul_info = manifest.XULInfo(xul_abi, xul_os, xul_debug)
        xul_tester = manifest.XULInfoTester(xul_info, options.js_shell)

    test_dir = dirname(abspath(__file__))
    test_list = manifest.load(test_dir, xul_tester)
    skip_list = []

    if options.make_manifests:
        manifest.make_manifests(options.make_manifests, test_list)
        sys.exit()

    # Create a new test list. Apply each JIT configuration to every test.
    if options.jitflags:
        new_test_list = []
        jitflags_list = parse_jitflags(options.jitflags)
        for test in test_list:
            for jitflags in jitflags_list:
                tmp_test = copy(test)
                tmp_test.options = copy(test.options)
                tmp_test.options.extend(jitflags)
                new_test_list.append(tmp_test)
        test_list = new_test_list

    if options.test_file:
        paths = set()
        for test_file in options.test_file:
            paths |= set([ line.strip() for line in open(test_file).readlines()])
        test_list = [ _ for _ in test_list if _.path in paths ]

    if requested_paths:
        def p(path):
            for arg in requested_paths:
                if path.find(arg) != -1:
                    return True
            return False
        test_list = [ _ for _ in test_list if p(_.path) ]

    if options.exclude_file:
        test_list = [_ for _ in test_list if _.path not in excluded_paths]

    if options.no_extensions:
        pattern = os.sep + 'extensions' + os.sep
        test_list = [_ for _ in test_list if pattern not in _.path]

    if not options.random:
        test_list = [ _ for _ in test_list if not _.random ]

    if options.run_only_skipped:
        options.run_skipped = True
        test_list = [ _ for _ in test_list if not _.enable ]

    if not options.run_slow_tests:
        test_list = [ _ for _ in test_list if not _.slow ]

    if not options.run_skipped:
        skip_list = [ _ for _ in test_list if not _.enable ]
        test_list = [ _ for _ in test_list if _.enable ]

    return skip_list, test_list

def main():
    options, requested_paths, excluded_paths = parse_args()
    skip_list, test_list = load_tests(options, requested_paths, excluded_paths)

    if not test_list:
        print 'no tests selected'
        return 1

    test_dir = dirname(abspath(__file__))

    if options.debug:
        if len(test_list) > 1:
            print('Multiple tests match command line arguments, debugger can only run one')
            for tc in test_list:
                print('    %s'%tc.path)
            return 2

        cmd = test_list[0].get_command(TestCase.js_cmd_prefix)
        if options.show_cmd:
            print list2cmdline(cmd)
        if test_dir not in ('', '.'):
            os.chdir(test_dir)
        call(cmd)
        return 0

    curdir = os.getcwd()
    if test_dir not in ('', '.'):
        os.chdir(test_dir)

    results = None
    try:
        results = ResultsSink(options, len(skip_list) + len(test_list))
        for t in skip_list:
            results.push(NullTestOutput(t))
        run_tests(options, test_list, results)
    finally:
        os.chdir(curdir)

    if results is None or not results.all_passed():
        return 1

    return 0

if __name__ == '__main__':
    sys.exit(main())
