#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
The JS Shell Test Harness.

See the adjacent README.txt for more details.
"""

from __future__ import print_function

import os
import sys
import textwrap
import platform
from os.path import abspath, dirname, isfile, realpath
from contextlib import contextmanager
from copy import copy
from subprocess import list2cmdline, call

from lib.tests import RefTestCase, get_jitflags, get_cpu_count, \
    get_environment_overlay, change_env
from lib.results import ResultsSink
from lib.progressbar import ProgressBar

if sys.platform.startswith('linux') or sys.platform.startswith('darwin'):
    from lib.tasks_unix import run_all_tests
else:
    from lib.tasks_win import run_all_tests


@contextmanager
def changedir(dirname):
    pwd = os.getcwd()
    os.chdir(dirname)
    try:
        yield
    finally:
        os.chdir(pwd)


class PathOptions(object):
    def __init__(self, location, requested_paths, excluded_paths):
        self.requested_paths = requested_paths
        self.excluded_files, self.excluded_dirs = PathOptions._split_files_and_dirs(
            location, excluded_paths)

    @staticmethod
    def _split_files_and_dirs(location, paths):
        """Split up a set of paths into files and directories"""
        files, dirs = set(), set()
        for path in paths:
            fullpath = os.path.join(location, path)
            if path.endswith('/'):
                dirs.add(path[:-1])
            elif os.path.isdir(fullpath):
                dirs.add(path)
            elif os.path.exists(fullpath):
                files.add(path)

        return files, dirs

    def should_run(self, filename):
        # If any tests are requested by name, skip tests that do not match.
        if self.requested_paths and not any(req in filename for req in self.requested_paths):
            return False

        # Skip excluded tests.
        if filename in self.excluded_files:
            return False

        for dir in self.excluded_dirs:
            if filename.startswith(dir + '/'):
                return False

        return True


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
                  help='config data for xulRuntime'
                  ' (avoids search for config/autoconf.mk)')

    harness_og = OptionGroup(op, "Harness Controls",
                             "Control how tests are run.")
    harness_og.add_option('-j', '--worker-count', type=int,
                          default=max(1, get_cpu_count()),
                          help='Number of tests to run in parallel'
                          ' (default %default)')
    harness_og.add_option('-t', '--timeout', type=float, default=150.0,
                          help='Set maximum time a test is allows to run'
                          ' (in seconds).')
    harness_og.add_option('--show-slow', action='store_true',
                          help='Show tests taking longer than a minimum time'
                          ' (in seconds).')
    harness_og.add_option('--slow-test-threshold', type=float, default=5.0,
                          help='Time in seconds a test can take until it is'
                          'considered slow (default %default).')
    harness_og.add_option('-a', '--args', dest='shell_args', default='',
                          help='Extra args to pass to the JS shell.')
    harness_og.add_option('--jitflags', dest='jitflags', default='none',
                          type='string',
                          help='IonMonkey option combinations. One of all,'
                          ' debug, ion, and none (default %default).')
    harness_og.add_option('--tbpl', action='store_true',
                          help='Runs each test in all configurations tbpl'
                          ' tests.')
    harness_og.add_option('--tbpl-debug', action='store_true',
                          help='Runs each test in some faster configurations'
                          ' tbpl tests.')
    harness_og.add_option('-g', '--debug', action='store_true',
                          help='Run a test in debugger.')
    harness_og.add_option('--debugger', default='gdb -q --args',
                          help='Debugger command.')
    harness_og.add_option('-J', '--jorendb', action='store_true',
                          help='Run under JS debugger.')
    harness_og.add_option('--passthrough', action='store_true',
                          help='Run tests with stdin/stdout attached to'
                          ' caller.')
    harness_og.add_option('--test-reflect-stringify', dest="test_reflect_stringify",
                          help="instead of running tests, use them to test the "
                          "Reflect.stringify code in specified file")
    harness_og.add_option('--valgrind', action='store_true',
                          help='Run tests in valgrind.')
    harness_og.add_option('--valgrind-args', default='',
                          help='Extra args to pass to valgrind.')
    harness_og.add_option('--rr', action='store_true',
                          help='Run tests under RR record-and-replay debugger.')
    harness_og.add_option('-C', '--check-output', action='store_true',
                          help='Run tests to check output for different jit-flags')
    op.add_option_group(harness_og)

    input_og = OptionGroup(op, "Inputs", "Change what tests are run.")
    input_og.add_option('-f', '--file', dest='test_file', action='append',
                        help='Get tests from the given file.')
    input_og.add_option('-x', '--exclude-file', action='append',
                        help='Exclude tests from the given file.')
    input_og.add_option('--include', action='append', dest='requested_paths', default=[],
                        help='Include the given test file or directory.')
    input_og.add_option('--exclude', action='append', dest='excluded_paths', default=[],
                        help='Exclude the given test file or directory.')
    input_og.add_option('-d', '--exclude-random', dest='random',
                        action='store_false',
                        help='Exclude tests marked as "random."')
    input_og.add_option('--run-skipped', action='store_true',
                        help='Run tests marked as "skip."')
    input_og.add_option('--run-only-skipped', action='store_true',
                        help='Run only tests marked as "skip."')
    input_og.add_option('--run-slow-tests', action='store_true',
                        help='Do not skip tests marked as "slow."')
    input_og.add_option('--no-extensions', action='store_true',
                        help='Run only tests conforming to the ECMAScript 5'
                        ' standard.')
    input_og.add_option('--repeat', type=int, default=1,
                        help='Repeat tests the given number of times.')
    op.add_option_group(input_og)

    output_og = OptionGroup(op, "Output",
                            "Modify the harness and tests output.")
    output_og.add_option('-s', '--show-cmd', action='store_true',
                         help='Show exact commandline used to run each test.')
    output_og.add_option('-o', '--show-output', action='store_true',
                         help="Print each test's output to the file given by"
                         " --output-file.")
    output_og.add_option('-F', '--failed-only', action='store_true',
                         help="If a --show-* option is given, only print"
                         " output for failed tests.")
    output_og.add_option('--no-show-failed', action='store_true',
                         help="Don't print output for failed tests"
                         " (no-op with --show-output).")
    output_og.add_option('-O', '--output-file',
                         help='Write all output to the given file'
                         ' (default: stdout).')
    output_og.add_option('--failure-file',
                         help='Write all not-passed tests to the given file.')
    output_og.add_option('--no-progress', dest='hide_progress',
                         action='store_true',
                         help='Do not show the progress bar.')
    output_og.add_option('--tinderbox', dest='format', action='store_const',
                         const='automation',
                         help='Use automation-parseable output format.')
    output_og.add_option('--format', dest='format', default='none',
                         type='choice', choices=['automation', 'none'],
                         help='Output format. Either automation or none'
                         ' (default %default).')
    op.add_option_group(output_og)

    special_og = OptionGroup(op, "Special",
                             "Special modes that do not run tests.")
    special_og.add_option('--make-manifests', metavar='BASE_TEST_PATH',
                          help='Generate reftest manifest files.')
    op.add_option_group(special_og)
    options, args = op.parse_args()

    # Acquire the JS shell given on the command line.
    options.js_shell = None
    requested_paths = set(options.requested_paths)
    if len(args) > 0:
        options.js_shell = abspath(args[0])
        requested_paths |= set(args[1:])

    # If we do not have a shell, we must be in a special mode.
    if options.js_shell is None and not options.make_manifests:
        op.error('missing JS_SHELL argument')

    # Valgrind, gdb, and rr are mutually exclusive.
    if sum(map(lambda e: 1 if e else 0, [options.valgrind, options.debug, options.rr])) > 1:
        op.error("--valgrind, --debug, and --rr are mutually exclusive.")

    # Fill the debugger field, as needed.
    if options.debug:
        if options.debugger == 'lldb':
            debugger_prefix = ['lldb', '--']
        else:
            debugger_prefix = options.debugger.split()
    else:
        debugger_prefix = []

    if options.valgrind:
        debugger_prefix = ['valgrind'] + options.valgrind_args.split()
        if os.uname()[0] == 'Darwin':
            debugger_prefix.append('--dsymutil=yes')
        options.show_output = True
    if options.rr:
        debugger_prefix = ['rr', 'record']

    js_cmd_args = options.shell_args.split()
    if options.jorendb:
        options.passthrough = True
        options.hide_progress = True
        options.worker_count = 1
        debugger_path = realpath(os.path.join(
            abspath(dirname(abspath(__file__))),
            '..', '..', 'examples', 'jorendb.js'))
        js_cmd_args.extend(['-d', '-f', debugger_path, '--'])
    prefix = RefTestCase.build_js_cmd_prefix(options.js_shell, js_cmd_args,
                                             debugger_prefix)

    # If files with lists of tests to run were specified, add them to the
    # requested tests set.
    if options.test_file:
        for test_file in options.test_file:
            requested_paths |= set(
                [line.strip() for line in open(test_file).readlines()])

    excluded_paths = set(options.excluded_paths)

    # If files with lists of tests to exclude were specified, add them to the
    # excluded tests set.
    if options.exclude_file:
        for filename in options.exclude_file:
            with open(filename, 'r') as fp:
                for line in fp:
                    if line.startswith('#'):
                        continue
                    line = line.strip()
                    if not line:
                        continue
                    excluded_paths.add(line)

    # Handle output redirection, if requested and relevant.
    options.output_fp = sys.stdout
    if options.output_file:
        if not options.show_cmd:
            options.show_output = True
        try:
            options.output_fp = open(options.output_file, 'w')
        except IOError as ex:
            raise SystemExit("Failed to open output file: " + str(ex))

    # Hide the progress bar if it will get in the way of other output.
    options.hide_progress = (options.format == 'automation' or
                             not ProgressBar.conservative_isatty() or
                             options.hide_progress)

    return (options, prefix, requested_paths, excluded_paths)


def load_tests(options, requested_paths, excluded_paths):
    """
    Returns a tuple: (test_count, test_gen)
        test_count: [int] Number of tests that will be in test_gen
        test_gen: [iterable<Test>] Tests found that should be run.
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
    path_options = PathOptions(test_dir, requested_paths, excluded_paths)
    test_count = manifest.count_tests(test_dir, path_options)
    test_gen = manifest.load_reftests(test_dir, path_options, xul_tester)

    if options.test_reflect_stringify is not None:
        def trs_gen(tests):
            for test in tests:
                test.test_reflect_stringify = options.test_reflect_stringify
                # Even if the test is not normally expected to pass, we still
                # expect reflect-stringify to be able to handle it.
                test.expect = True
                test.random = False
                test.slow = False
                yield test
        test_gen = trs_gen(test_gen)

    if options.make_manifests:
        manifest.make_manifests(options.make_manifests, test_gen)
        sys.exit()

    # Create a new test list. Apply each TBPL configuration to every test.
    flags_list = None
    if options.tbpl:
        flags_list = get_jitflags('all')
    elif options.tbpl_debug:
        flags_list = get_jitflags('debug')
    else:
        flags_list = get_jitflags(options.jitflags, none=None)

    if flags_list:
        def flag_gen(tests):
            for test in tests:
                for jitflags in flags_list:
                    tmp_test = copy(test)
                    tmp_test.jitflags = copy(test.jitflags)
                    tmp_test.jitflags.extend(jitflags)
                    yield tmp_test
        test_count = test_count * len(flags_list)
        test_gen = flag_gen(test_gen)

    if options.test_file:
        paths = set()
        for test_file in options.test_file:
            paths |= set(
                [line.strip() for line in open(test_file).readlines()])
        test_gen = (_ for _ in test_gen if _.path in paths)

    if options.no_extensions:
        pattern = os.sep + 'extensions' + os.sep
        test_gen = (_ for _ in test_gen if pattern not in _.path)

    if not options.random:
        test_gen = (_ for _ in test_gen if not _.random)

    if options.run_only_skipped:
        options.run_skipped = True
        test_gen = (_ for _ in test_gen if not _.enable)

    if not options.run_slow_tests:
        test_gen = (_ for _ in test_gen if not _.slow)

    if options.repeat:
        test_gen = (test for test in test_gen for i in range(options.repeat))
        test_count *= options.repeat

    return test_count, test_gen


def main():
    options, prefix, requested_paths, excluded_paths = parse_args()
    if options.js_shell is not None and not (isfile(options.js_shell) and
                                             os.access(options.js_shell, os.X_OK)):
        if (platform.system() != 'Windows' or
            isfile(options.js_shell) or not
            isfile(options.js_shell + ".exe") or not
                os.access(options.js_shell + ".exe", os.X_OK)):
            print('Could not find executable shell: ' + options.js_shell)
            return 1

    test_count, test_gen = load_tests(options, requested_paths, excluded_paths)
    test_environment = get_environment_overlay(options.js_shell)

    if test_count == 0:
        print('no tests selected')
        return 1

    test_dir = dirname(abspath(__file__))

    if options.debug:
        if test_count > 1:
            print('Multiple tests match command line arguments,'
                  ' debugger can only run one')
            for tc in test_gen:
                print('    {}'.format(tc.path))
            return 2

        cmd = test_gen.next().get_command(prefix)
        if options.show_cmd:
            print(list2cmdline(cmd))
        with changedir(test_dir), change_env(test_environment):
            call(cmd)
        return 0

    with changedir(test_dir), change_env(test_environment):
        results = ResultsSink('jstests', options, test_count)
        try:
            for out in run_all_tests(test_gen, prefix, results.pb, options):
                results.push(out)
            results.finish(True)
        except KeyboardInterrupt:
            results.finish(False)

        return 0 if results.all_passed() else 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
