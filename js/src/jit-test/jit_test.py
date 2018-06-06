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
import shutil
import subprocess
import sys
import traceback


def add_libdir_to_path():
    from os.path import dirname, exists, join, realpath
    js_src_dir = dirname(dirname(realpath(sys.argv[0])))
    assert exists(join(js_src_dir, 'jsapi.h'))
    sys.path.insert(0, join(js_src_dir, 'lib'))
    sys.path.insert(0, join(js_src_dir, 'tests', 'lib'))


add_libdir_to_path()

import jittests
from tests import get_jitflags, valid_jitflags, get_cpu_count, get_environment_overlay, \
    change_env

# Python 3.3 added shutil.which, but we can't use that yet.


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

    item = raw_input('Which one:\n')
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
    from optparse import OptionParser
    op = OptionParser(usage='%prog [options] JS_SHELL [TESTS]')
    op.add_option('-s', '--show-cmd', dest='show_cmd', action='store_true',
                  help='show js shell command run')
    op.add_option('-f', '--show-failed-cmd', dest='show_failed',
                  action='store_true',
                  help='show command lines of failed tests')
    op.add_option('-o', '--show-output', dest='show_output',
                  action='store_true',
                  help='show output from js shell')
    op.add_option('-F', '--failed-only', dest='failed_only',
                  action='store_true',
                  help="if --show-output is given, only print output for"
                  " failed tests")
    op.add_option('--no-show-failed', dest='no_show_failed',
                  action='store_true',
                  help="don't print output for failed tests"
                  " (no-op with --show-output)")
    op.add_option('-x', '--exclude', dest='exclude',
                  default=[], action='append',
                  help='exclude given test dir or path')
    op.add_option('--exclude-from', dest='exclude_from', type=str,
                  help='exclude each test dir or path in FILE')
    op.add_option('--slow', dest='run_slow', action='store_true',
                  help='also run tests marked as slow')
    op.add_option('--no-slow', dest='run_slow', action='store_false',
                  help='do not run tests marked as slow (the default)')
    op.add_option('-t', '--timeout', dest='timeout', type=float, default=150.0,
                  help='set test timeout in seconds')
    op.add_option('--no-progress', dest='hide_progress', action='store_true',
                  help='hide progress bar')
    op.add_option('--tinderbox', dest='format', action='store_const',
                  const='automation',
                  help='Use automation-parseable output format')
    op.add_option('--format', dest='format', default='none', type='choice',
                  choices=['automation', 'none'],
                  help='Output format. Either automation or none'
                  ' (default %default).')
    op.add_option('--args', dest='shell_args', default='',
                  help='extra args to pass to the JS shell')
    op.add_option('-w', '--write-failures', dest='write_failures',
                  metavar='FILE',
                  help='Write a list of failed tests to [FILE]')
    op.add_option('-C', '--check-output', action='store_true', dest='check_output',
                  help='Run tests to check output for different jit-flags')
    op.add_option('-r', '--read-tests', dest='read_tests', metavar='FILE',
                  help='Run test files listed in [FILE]')
    op.add_option('-R', '--retest', dest='retest', metavar='FILE',
                  help='Retest using test list file [FILE]')
    op.add_option('-g', '--debug', action='store_const', const='gdb', dest='debugger',
                  help='Run a single test under the gdb debugger')
    op.add_option('-G', '--debug-rr', action='store_const', const='rr', dest='debugger',
                  help='Run a single test under the rr debugger')
    op.add_option('--debugger', type='string',
                  help='Run a single test under the specified debugger')
    op.add_option('--valgrind', dest='valgrind', action='store_true',
                  help='Enable the |valgrind| flag, if valgrind is in $PATH.')
    op.add_option('--unusable-error-status', action='store_true',
                  help='Ignore incorrect exit status on tests that should return nonzero.')
    op.add_option('--valgrind-all', dest='valgrind_all', action='store_true',
                  help='Run all tests with valgrind, if valgrind is in $PATH.')
    op.add_option('--avoid-stdio', dest='avoid_stdio', action='store_true',
                  help='Use js-shell file indirection instead of piping stdio.')
    op.add_option('--write-failure-output', dest='write_failure_output',
                  action='store_true',
                  help='With --write-failures=FILE, additionally write the'
                  ' output of failed tests to [FILE]')
    op.add_option('--jitflags', dest='jitflags', default='none',
                  choices=valid_jitflags(),
                  help='IonMonkey option combinations. One of %s.' % ', '.join(valid_jitflags()))
    op.add_option('--ion', dest='jitflags', action='store_const', const='ion',
                  help='Run tests once with --ion-eager and once with'
                  ' --baseline-eager (equivalent to --jitflags=ion)')
    op.add_option('--tbpl', dest='jitflags', action='store_const', const='all',
                  help='Run tests with all IonMonkey option combinations'
                  ' (equivalent to --jitflags=all)')
    op.add_option('-j', '--worker-count', dest='max_jobs', type=int,
                  default=max(1, get_cpu_count()),
                  help='Number of tests to run in parallel (default %default)')
    op.add_option('--remote', action='store_true',
                  help='Run tests on a remote device')
    op.add_option('--deviceIP', action='store',
                  type='string', dest='device_ip',
                  help='IP address of remote device to test')
    op.add_option('--devicePort', action='store',
                  type=int, dest='device_port', default=20701,
                  help='port of remote device to test')
    op.add_option('--deviceSerial', action='store',
                  type='string', dest='device_serial', default=None,
                  help='ADB device serial number of remote device to test')
    op.add_option('--remoteTestRoot', dest='remote_test_root', action='store',
                  type='string', default='/data/local/tests',
                  help='The remote directory to use as test root'
                  ' (eg. /data/local/tests)')
    op.add_option('--localLib', dest='local_lib', action='store',
                  type='string',
                  help='The location of libraries to push -- preferably'
                  ' stripped')
    op.add_option('--repeat', type=int, default=1,
                  help='Repeat tests the given number of times.')
    op.add_option('--this-chunk', type=int, default=1,
                  help='The test chunk to run.')
    op.add_option('--total-chunks', type=int, default=1,
                  help='The total number of test chunks.')
    op.add_option('--ignore-timeouts', dest='ignore_timeouts', metavar='FILE',
                  help='Ignore timeouts of tests listed in [FILE]')
    op.add_option('--test-reflect-stringify', dest="test_reflect_stringify",
                  help="instead of running tests, use them to test the "
                  "Reflect.stringify code in specified file")

    options, args = op.parse_args(argv)
    if len(args) < 1:
        op.error('missing JS_SHELL argument')
    js_shell = which(args[0])
    test_args = args[1:]
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

    # No point in adding in noasmjs and wasm-baseline variants if the
    # jitflags forbid asmjs in the first place. (This is to avoid getting a
    # wasm-baseline run when requesting --jitflags=interp, but the test
    # contains test-also-noasmjs.)
    test_flags = get_jitflags(options.jitflags)
    options.asmjs_enabled = True
    options.wasm_enabled = True
    if all(['--no-asmjs' in flags for flags in test_flags]):
        options.asmjs_enabled = False
        options.wasm_enabled = False
    if all(['--no-wasm' in flags for flags in test_flags]):
        options.asmjs_enabled = False
        options.wasm_enabled = False

    if test_args:
        read_all = False
        for arg in test_args:
            test_list += jittests.find_tests(arg)

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
        test_list = jittests.find_tests()

    # Exclude tests when code coverage is enabled.
    # This part is equivalent to:
    # skip-if = coverage
    if os.getenv('GCOV_PREFIX') is not None:
        # GCOV errors.
        options.exclude += [os.path.join('asm.js', 'testSIMD.js')]               # Bug 1347245

        # JSVM errors.
        options.exclude += [os.path.join('basic', 'functionnames.js')]           # Bug 1369783
        options.exclude += [os.path.join('debug', 'Debugger-findScripts-23.js')]
        options.exclude += [os.path.join('debug', 'bug1160182.js')]
        options.exclude += [os.path.join('xdr', 'incremental-encoder.js')]
        options.exclude += [os.path.join('xdr', 'bug1186973.js')]                # Bug 1369785
        options.exclude += [os.path.join('xdr', 'relazify.js')]
        options.exclude += [os.path.join('basic', 'werror.js')]

        # Prevent code coverage test that expects coverage
        # to be off when it starts.
        options.exclude += [os.path.join('debug', 'Script-getOffsetsCoverage-02.js')]

    if options.exclude_from:
        with open(options.exclude_from) as fh:
            for line in fh:
                line_exclude = line.strip()
                if not line_exclude.startswith("#") and len(line_exclude):
                    options.exclude.append(line_exclude)

    if options.exclude:
        exclude_list = []
        for exclude in options.exclude:
            exclude_list += jittests.find_tests(exclude)
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
                options.ignore_timeouts = set(
                    [line.strip('\n') for line in f.readlines()])
        except IOError:
            sys.exit("Error reading file: " + options.ignore_timeouts)
    else:
        options.ignore_timeouts = set()

    prefix = [js_shell] + shlex.split(options.shell_args)
    prologue = os.path.join(jittests.LIB_DIR, 'prologue.js')
    if options.remote:
        prologue = posixpath.join(options.remote_test_root,
                                  'jit-tests', 'jit-tests', 'lib', 'prologue.js')

    prefix += ['-f', prologue]

    # Clean up any remnants from previous crashes etc
    shutil.rmtree(jittests.JS_CACHE_DIR, ignore_errors=True)
    os.mkdir(jittests.JS_CACHE_DIR)

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
            tc = job_list.next()

        if options.debugger == 'gdb':
            debug_cmd = ['gdb', '--args']
        elif options.debugger == 'lldb':
            debug_cmd = ['lldb', '--']
        elif options.debugger == 'rr':
            debug_cmd = ['rr', 'record']
        else:
            debug_cmd = options.debugger.split()

        with change_env(test_environment):
            subprocess.call(debug_cmd + tc.command(prefix, jittests.LIB_DIR, jittests.MODULE_DIR))
            if options.debugger == 'rr':
                subprocess.call(['rr', 'replay'])
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
