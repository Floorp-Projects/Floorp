#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import math
import os
import platform
import posixpath
import shlex
import subprocess
import sys
import traceback


read_input = input
if sys.version_info.major == 2:
    read_input = raw_input


def add_tests_dir_to_path():
    from os.path import dirname, exists, join, realpath
    js_src_dir = dirname(dirname(realpath(sys.argv[0])))
    assert exists(join(js_src_dir, 'jsapi.h'))
    sys.path.insert(0, join(js_src_dir, 'tests'))


add_tests_dir_to_path()

from lib import jittests
from lib.tests import (
    get_jitflags,
    valid_jitflags,
    get_cpu_count,
    get_environment_overlay,
    change_env,
)


def which(name):
    if name.find(os.path.sep) != -1:
        return os.path.abspath(name)

    for path in os.environ["PATH"].split(os.pathsep):
        full = os.path.join(path, name)
        if os.path.exists(full):
            return os.path.abspath(full)

    return name


def choose_item(jobs, max_items, display):
    job_count = len(jobs)

    # Don't present a choice if there are too many tests
    if job_count > max_items:
        raise Exception('Too many jobs.')

    for i, job in enumerate(jobs, 1):
        print("{}) {}".format(i, display(job)))

    item = read_input('Which one:\n')
    try:
        item = int(item)
        if item > job_count or item < 1:
            raise Exception('Input isn\'t between 1 and {}'.format(job_count))
    except ValueError:
        raise Exception('Unrecognized input')

    return jobs[item - 1]


def main(argv):
    # The [TESTS] optional arguments are paths of test files relative
    # to the jit-test/tests directory.
    import argparse
    op = argparse.ArgumentParser(description='Run jit-test JS shell tests')
    op.add_argument('-s', '--show-cmd', dest='show_cmd', action='store_true',
                    help='show js shell command run')
    op.add_argument('-f', '--show-failed-cmd', dest='show_failed',
                    action='store_true',
                    help='show command lines of failed tests')
    op.add_argument('-o', '--show-output', dest='show_output',
                    action='store_true',
                    help='show output from js shell')
    op.add_argument('-F', '--failed-only', dest='failed_only',
                    action='store_true',
                    help="if --show-output is given, only print output for"
                    " failed tests")
    op.add_argument('--no-show-failed', dest='no_show_failed',
                    action='store_true',
                    help="don't print output for failed tests"
                    " (no-op with --show-output)")
    op.add_argument('-x', '--exclude', dest='exclude',
                    default=[], action='append',
                    help='exclude given test dir or path')
    op.add_argument('--exclude-from', dest='exclude_from', type=str,
                    help='exclude each test dir or path in FILE')
    op.add_argument('--slow', dest='run_slow', action='store_true',
                    help='also run tests marked as slow')
    op.add_argument('--no-slow', dest='run_slow', action='store_false',
                    help='do not run tests marked as slow (the default)')
    op.add_argument('-t', '--timeout', dest='timeout', type=float, default=150.0,
                    help='set test timeout in seconds')
    op.add_argument('--no-progress', dest='hide_progress', action='store_true',
                    help='hide progress bar')
    op.add_argument('--tinderbox', dest='format', action='store_const',
                    const='automation',
                    help='Use automation-parseable output format')
    op.add_argument('--format', dest='format', default='none',
                    choices=('automation', 'none'),
                    help='Output format (default %(default)s).')
    op.add_argument('--args', dest='shell_args', metavar='ARGS', default='',
                    help='extra args to pass to the JS shell')
    op.add_argument('--feature-args', dest='feature_args', metavar='ARGS',
                    default='',
                    help='even more args to pass to the JS shell '
                    '(for compatibility with jstests.py)')
    op.add_argument('-w', '--write-failures', dest='write_failures',
                    metavar='FILE',
                    help='Write a list of failed tests to [FILE]')
    op.add_argument('-C', '--check-output', action='store_true', dest='check_output',
                    help='Run tests to check output for different jit-flags')
    op.add_argument('-r', '--read-tests', dest='read_tests', metavar='FILE',
                    help='Run test files listed in [FILE]')
    op.add_argument('-R', '--retest', dest='retest', metavar='FILE',
                    help='Retest using test list file [FILE]')
    op.add_argument('-g', '--debug', action='store_const', const='gdb', dest='debugger',
                    help='Run a single test under the gdb debugger')
    op.add_argument('-G', '--debug-rr', action='store_const', const='rr', dest='debugger',
                    help='Run a single test under the rr debugger')
    op.add_argument('--debugger', type=str,
                    help='Run a single test under the specified debugger')
    op.add_argument('--valgrind', dest='valgrind', action='store_true',
                    help='Enable the |valgrind| flag, if valgrind is in $PATH.')
    op.add_argument('--unusable-error-status', action='store_true',
                    help='Ignore incorrect exit status on tests that should return nonzero.')
    op.add_argument('--valgrind-all', dest='valgrind_all', action='store_true',
                    help='Run all tests with valgrind, if valgrind is in $PATH.')
    op.add_argument('--avoid-stdio', dest='avoid_stdio', action='store_true',
                    help='Use js-shell file indirection instead of piping stdio.')
    op.add_argument('--write-failure-output', dest='write_failure_output',
                    action='store_true',
                    help='With --write-failures=FILE, additionally write the'
                    ' output of failed tests to [FILE]')
    op.add_argument('--jitflags', dest='jitflags', default='none',
                    choices=valid_jitflags(),
                    help='IonMonkey option combinations (default %(default)s).')
    op.add_argument('--ion', dest='jitflags', action='store_const', const='ion',
                    help='Run tests once with --ion-eager and once with'
                    ' --baseline-eager (equivalent to --jitflags=ion)')
    op.add_argument('--tbpl', dest='jitflags', action='store_const', const='all',
                    help='Run tests with all IonMonkey option combinations'
                    ' (equivalent to --jitflags=all)')
    op.add_argument('-j', '--worker-count', dest='max_jobs', type=int,
                    default=max(1, get_cpu_count()),
                    help='Number of tests to run in parallel (default %(default)s).')
    op.add_argument('--remote', action='store_true',
                    help='Run tests on a remote device')
    op.add_argument('--deviceIP', action='store',
                    type=str, dest='device_ip',
                    help='IP address of remote device to test')
    op.add_argument('--devicePort', action='store',
                    type=int, dest='device_port', default=20701,
                    help='port of remote device to test')
    op.add_argument('--deviceSerial', action='store',
                    type=str, dest='device_serial', default=None,
                    help='ADB device serial number of remote device to test')
    op.add_argument('--remoteTestRoot', dest='remote_test_root', action='store',
                    type=str, default='/data/local/tests',
                    help='The remote directory to use as test root'
                    ' (e.g.  %(default)s)')
    op.add_argument('--localLib', dest='local_lib', action='store',
                    type=str,
                    help='The location of libraries to push -- preferably'
                    ' stripped')
    op.add_argument('--repeat', type=int, default=1,
                    help='Repeat tests the given number of times.')
    op.add_argument('--this-chunk', type=int, default=1,
                    help='The test chunk to run.')
    op.add_argument('--total-chunks', type=int, default=1,
                    help='The total number of test chunks.')
    op.add_argument('--ignore-timeouts', dest='ignore_timeouts', metavar='FILE',
                    help='Ignore timeouts of tests listed in [FILE]')
    op.add_argument('--test-reflect-stringify', dest="test_reflect_stringify",
                    help="instead of running tests, use them to test the "
                    "Reflect.stringify code in specified file")
    op.add_argument('--run-binast', action='store_true',
                    dest="run_binast",
                    help="By default BinAST testcases encoded from JS "
                    "testcases are skipped. If specified, BinAST testcases "
                    "are also executed.")
    # --enable-webrender is ignored as it is not relevant for JIT
    # tests, but is required for harness compatibility.
    op.add_argument('--enable-webrender', action='store_true',
                    dest="enable_webrender", default=False,
                    help=argparse.SUPPRESS)
    op.add_argument('js_shell', metavar='JS_SHELL', help='JS shell to run tests with')

    options, test_args = op.parse_known_args(argv)
    js_shell = which(options.js_shell)
    test_environment = get_environment_overlay(js_shell)

    if not (os.path.isfile(js_shell) and os.access(js_shell, os.X_OK)):
        if (platform.system() != 'Windows' or
            os.path.isfile(js_shell) or not
            os.path.isfile(js_shell + ".exe") or not
                os.access(js_shell + ".exe", os.X_OK)):
            op.error('shell is not executable: ' + js_shell)

    if jittests.stdio_might_be_broken():
        # Prefer erring on the side of caution and not using stdio if
        # it might be broken on this platform.  The file-redirect
        # fallback should work on any platform, so at worst by
        # guessing wrong we might have slowed down the tests a bit.
        #
        # XXX technically we could check for broken stdio, but it
        # really seems like overkill.
        options.avoid_stdio = True

    if options.retest:
        options.read_tests = options.retest
        options.write_failures = options.retest

    test_list = []
    read_all = True

    if options.run_binast:
        code = 'print(getBuildConfiguration().binast)'
        is_binast_enabled = subprocess.check_output(
            [js_shell, '-e', code]
        ).decode(errors='replace')
        if not is_binast_enabled.startswith('true'):
            print("While --run-binast is specified, BinAST is not enabled.",
                  file=sys.stderr)
            print("BinAST testcases will be skipped.",
                  file=sys.stderr)
            options.run_binast = False

    if test_args:
        read_all = False
        for arg in test_args:
            test_list += jittests.find_tests(arg, run_binast=options.run_binast)

    if options.read_tests:
        read_all = False
        try:
            f = open(options.read_tests)
            for line in f:
                test_list.append(os.path.join(jittests.TEST_DIR,
                                              line.strip('\n')))
            f.close()
        except IOError:
            if options.retest:
                read_all = True
            else:
                sys.stderr.write("Exception thrown trying to read test file"
                                 " '{}'\n".format(options.read_tests))
                traceback.print_exc()
                sys.stderr.write('---\n')

    if read_all:
        test_list = jittests.find_tests(run_binast=options.run_binast)

    if options.exclude_from:
        with open(options.exclude_from) as fh:
            for line in fh:
                line_exclude = line.strip()
                if not line_exclude.startswith("#") and len(line_exclude):
                    options.exclude.append(line_exclude)

    if options.exclude:
        exclude_list = []
        for exclude in options.exclude:
            exclude_list += jittests.find_tests(exclude, run_binast=options.run_binast)
        test_list = [test for test in test_list
                     if test not in set(exclude_list)]

    if not test_list:
        print("No tests found matching command line arguments.",
              file=sys.stderr)
        sys.exit(0)

    test_list = [jittests.JitTest.from_file(_, options) for _ in test_list]

    if not options.run_slow:
        test_list = [_ for _ in test_list if not _.slow]

    if options.test_reflect_stringify is not None:
        for test in test_list:
            test.test_reflect_stringify = options.test_reflect_stringify

    # If chunking is enabled, determine which tests are part of this chunk.
    # This code was adapted from testing/mochitest/runtestsremote.py.
    if options.total_chunks > 1:
        total_tests = len(test_list)
        tests_per_chunk = math.ceil(total_tests / float(options.total_chunks))
        start = int(round((options.this_chunk - 1) * tests_per_chunk))
        end = int(round(options.this_chunk * tests_per_chunk))
        test_list = test_list[start:end]

    if not test_list:
        print("No tests found matching command line arguments after filtering.",
              file=sys.stderr)
        sys.exit(0)

    # The full test list is ready. Now create copies for each JIT configuration.
    test_flags = get_jitflags(options.jitflags)

    test_list = [_ for test in test_list for _ in test.copy_variants(test_flags)]

    job_list = (test for test in test_list)
    job_count = len(test_list)

    if options.repeat:
        job_list = (test for test in job_list for i in range(options.repeat))
        job_count *= options.repeat

    if options.ignore_timeouts:
        read_all = False
        try:
            with open(options.ignore_timeouts) as f:
                ignore = set()
                for line in f.readlines():
                    path = line.strip('\n')
                    ignore.add(path)

                    binjs_path = path.replace('.js', '.binjs')
                    # Do not use os.path.join to always use '/'.
                    ignore.add('binast/nonlazy/{}'.format(binjs_path))
                    ignore.add('binast/lazy/{}'.format(binjs_path))
                options.ignore_timeouts = ignore
        except IOError:
            sys.exit("Error reading file: " + options.ignore_timeouts)
    else:
        options.ignore_timeouts = set()

    prefix = [js_shell] + shlex.split(options.shell_args) + shlex.split(options.feature_args)
    prologue = os.path.join(jittests.LIB_DIR, 'prologue.js')
    if options.remote:
        prologue = posixpath.join(options.remote_test_root,
                                  'tests', 'tests', 'lib', 'prologue.js')

    prefix += ['-f', prologue]

    if options.debugger:
        if job_count > 1:
            print('Multiple tests match command line'
                  ' arguments, debugger can only run one')
            jobs = list(job_list)

            def display_job(job):
                flags = ""
                if len(job.jitflags) != 0:
                    flags = "({})".format(' '.join(job.jitflags))
                return '{} {}'.format(job.path, flags)

            try:
                tc = choose_item(jobs, max_items=50, display=display_job)
            except Exception as e:
                sys.exit(str(e))
        else:
            tc = next(job_list)

        if options.debugger == 'gdb':
            debug_cmd = ['gdb', '--args']
        elif options.debugger == 'lldb':
            debug_cmd = ['lldb', '--']
        elif options.debugger == 'rr':
            debug_cmd = ['rr', 'record']
        else:
            debug_cmd = options.debugger.split()

        with change_env(test_environment):
            if options.debugger == 'rr':
                subprocess.call(debug_cmd +
                                tc.command(prefix, jittests.LIB_DIR, jittests.MODULE_DIR))
                os.execvp('rr', ['rr', 'replay'])
            else:
                os.execvp(debug_cmd[0], debug_cmd +
                          tc.command(prefix, jittests.LIB_DIR, jittests.MODULE_DIR))
        sys.exit()

    try:
        ok = None
        if options.remote:
            ok = jittests.run_tests(job_list, job_count, prefix, options, remote=True)
        else:
            with change_env(test_environment):
                ok = jittests.run_tests(job_list, job_count, prefix, options)
        if not ok:
            sys.exit(2)
    except OSError:
        if not os.path.exists(prefix[0]):
            print("JS shell argument: file does not exist:"
                  " '{}'".format(prefix[0]), file=sys.stderr)
            sys.exit(1)
        else:
            raise


if __name__ == '__main__':
    main(sys.argv[1:])
