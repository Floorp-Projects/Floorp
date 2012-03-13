#!/usr/bin/env python

# Test harness for JSTests, controlled by manifest files.

import datetime, os, sys
from subprocess import list2cmdline, call

from results import NullTestOutput
from tests import TestCase
from tasks_win import Source
from progressbar import ProgressBar
from results import ResultsSink

if (sys.platform.startswith('linux') or
    sys.platform.startswith('darwin')
   ):
    from tasks_unix import run_all_tests
else:
    from tasks_win import run_all_tests

def exclude_tests(test_list, exclude_files):
    exclude_paths = []
    for filename in exclude_files:
        for line in open(filename):
            if line.startswith('#'): continue
            line = line.strip('\n')
            if not line: continue
            exclude_paths.append(line)
    return [ _ for _ in test_list if _.path not in exclude_paths ]

def run_tests(tests, results):
    """Run the given tests, sending raw results to the given results accumulator."""
    pb = None
    if not OPTIONS.hide_progress:
        try:
            from progressbar import ProgressBar
            pb = ProgressBar('', len(tests), 16)
        except ImportError:
            pass
    results.pb = pb

    try:
        results.finished = run_all_tests(tests, results, OPTIONS)
    except KeyboardInterrupt:
        results.finished = False

    if pb:
        pb.finish()

    if not OPTIONS.tinderbox:
        results.list()

if __name__ == '__main__':
    from optparse import OptionParser
    op = OptionParser(usage='%prog JS_SHELL [TEST-SPECS]')
    op.add_option('-s', '--show-cmd', dest='show_cmd', action='store_true',
                  help='show js shell command run')
    op.add_option('-o', '--show-output', dest='show_output', action='store_true',
                  help='show output from js shell')
    op.add_option('-O', '--output-file', dest='output_file',
                  help='write command output to the given file')
    op.add_option('-f', '--file', dest='test_file', action='append',
                  help='get tests from the given file')
    op.add_option('-x', '--exclude-file', dest='exclude_file', action='append',
                  help='exclude tests from the given file')
    op.add_option('--no-progress', dest='hide_progress', action='store_true',
                  help='hide progress bar')
    op.add_option('-j', '--worker-count', dest='worker_count', type=int, default=2,
                  help='number of worker threads to run tests on (default 2)')
    op.add_option('-m', '--manifest', dest='manifest',
                  help='select manifest file')
    op.add_option('-t', '--timeout', dest='timeout', type=float, default=150.0,
                  help='set test timeout in seconds')
    op.add_option('-d', '--exclude-random', dest='random', action='store_false',
                  help='exclude tests marked random', default=True)
    op.add_option('--run-skipped', dest='run_skipped', action='store_true',
                  help='run skipped tests')
    op.add_option('--run-only-skipped', dest='run_only_skipped', action='store_true',
                  help='run only skipped tests')
    op.add_option('--tinderbox', dest='tinderbox', action='store_true',
                  help='Tinderbox-parseable output format')
    op.add_option('--args', dest='shell_args', default='',
                  help='extra args to pass to the JS shell')
    op.add_option('-g', '--debug', dest='debug', action='store_true',
                  help='run test in debugger')
    op.add_option('--debugger', dest='debugger', default='gdb -q --args',
                  help='debugger command')
    op.add_option('--valgrind', dest='valgrind', action='store_true',
                  help='run tests in valgrind')
    op.add_option('--valgrind-args', dest='valgrind_args',
                  help='extra args to pass to valgrind')
    op.add_option('-c', '--check-manifest', dest='check_manifest', action='store_true',
                  help='check for test files not listed in the manifest')
    op.add_option('--failure-file', dest='failure_file',
                  help='write tests that have not passed to the given file')
    op.add_option('--run-slow-tests', dest='run_slow_tests', action='store_true',
                  help='run particularly slow tests as well as average-speed tests')
    op.add_option('--xul-info', dest='xul_info_src',
                  help='config data for xulRuntime (avoids search for config/autoconf.mk)')
    op.add_option('--no-extensions', dest='no_extensions', action='store_true',
                  help='run only tests conforming to the ECMAScript 5 standard')
    (OPTIONS, args) = op.parse_args()
    if len(args) < 1:
        if not OPTIONS.check_manifest:
            op.error('missing JS_SHELL argument')
        JS, args = None, []
    else:
        JS, args = args[0], args[1:]
    # Convert to an absolute path so we can run JS from a different directory.
    if JS is not None:
        JS = os.path.abspath(JS)

    if OPTIONS.debug:
        if OPTIONS.valgrind:
            print >> sys.stderr, "--debug and --valgrind options are mutually exclusive"
            sys.exit(2)
        debugger_prefix = OPTIONS.debugger.split(' ')
    elif OPTIONS.valgrind:
        debugger_prefix = ['valgrind']
        if os.uname()[0] == 'Darwin':
            debugger_prefix.append('--dsymutil=yes')
        if OPTIONS.valgrind_args:
            debugger_prefix.append(OPTIONS.valgrind_args)
        # Running under valgrind is not very useful if we don't show results.
        OPTIONS.show_output = True
    else:
        debugger_prefix = []

    TestCase.set_js_cmd_prefix(JS, OPTIONS.shell_args.split(), debugger_prefix)

    output_file = sys.stdout
    if OPTIONS.output_file and (OPTIONS.show_cmd or OPTIONS.show_output):
        output_file = open(OPTIONS.output_file, 'w')
    ResultsSink.output_file = output_file

    if ((OPTIONS.show_cmd or OPTIONS.show_output) and
        output_file == sys.stdout or OPTIONS.tinderbox):
        OPTIONS.hide_progress = True

    if OPTIONS.manifest is None:
        filename = os.path.join(os.path.dirname(__file__), 'jstests.list')
        if os.path.isfile(filename):
            OPTIONS.manifest = filename
        else:
            print >> sys.stderr, 'no manifest file given and defaults not found'
            sys.exit(2)

    import manifest
    if JS is None:
        xul_tester = manifest.NullXULInfoTester()
    else:
        if OPTIONS.xul_info_src is None:
            xul_info = manifest.XULInfo.create(JS)
        else:
            xul_abi, xul_os, xul_debug = OPTIONS.xul_info_src.split(r':')
            xul_debug = xul_debug.lower() is 'true'
            xul_info = manifest.XULInfo(xul_abi, xul_os, xul_debug)
        xul_tester = manifest.XULInfoTester(xul_info, JS)
    test_list = manifest.parse(OPTIONS.manifest, xul_tester)
    skipped_list = []

    if OPTIONS.check_manifest:
        manifest.check_manifest(test_list)
        if JS is None:
            sys.exit()

    if OPTIONS.test_file:
        paths = set()
        for test_file in OPTIONS.test_file:
            paths |= set([ line.strip() for line in open(test_file).readlines()])
        test_list = [ _ for _ in test_list if _.path in paths ]

    if args:
        def p(path):
            for arg in args:
                if path.find(arg) != -1:
                    return True
            return False
        test_list = [ _ for _ in test_list if p(_.path) ]

    if OPTIONS.exclude_file:
        test_list = exclude_tests(test_list, OPTIONS.exclude_file)

    if OPTIONS.no_extensions:
        pattern = os.sep + 'extensions' + os.sep
        test_list = [_ for _ in test_list if pattern not in _.path]

    if not OPTIONS.random:
        test_list = [ _ for _ in test_list if not _.random ]

    if OPTIONS.run_only_skipped:
        OPTIONS.run_skipped = True
        test_list = [ _ for _ in test_list if not _.enable ]

    if not OPTIONS.run_slow_tests:
        test_list = [ _ for _ in test_list if not _.slow ]

    if not OPTIONS.run_skipped:
        skipped_list = [ _ for _ in test_list if not _.enable ]
        test_list = [ _ for _ in test_list if _.enable ]

    if not test_list:
        print 'no tests selected'
        sys.exit(1)

    if OPTIONS.debug:
        if len(test_list) > 1:
            print('Multiple tests match command line arguments, debugger can only run one')
            for tc in test_list:
                print('    %s'%tc.path)
            sys.exit(2)

        cmd = test_list[0].get_command(TestCase.js_cmd_prefix)
        if OPTIONS.show_cmd:
            print list2cmdline(cmd)
        manifest_dir = os.path.dirname(OPTIONS.manifest)
        if manifest_dir not in ('', '.'):
            os.chdir(manifest_dir)
        call(cmd)
        sys.exit()

    curdir = os.getcwd()
    manifest_dir = os.path.dirname(OPTIONS.manifest)
    if manifest_dir not in ('', '.'):
        os.chdir(manifest_dir)

    results = None
    try:
        results = ResultsSink(output_file, OPTIONS)
        run_tests(test_list, results)
    finally:
        os.chdir(curdir)

    for t in skipped_list:
        results.push(NullTestOutput(t))

    if output_file != sys.stdout:
        output_file.close()

    if results is None or not results.all_passed():
        sys.exit(1)
