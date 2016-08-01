#!/usr/bin/env python

import argparse
import json
import logging
import re
import os
import platform
import posixpath
import shutil
import subprocess
import sys

from collections import Counter, namedtuple
from os import environ as env
from subprocess import Popen
from threading import Timer

Dirs = namedtuple('Dirs', ['scripts', 'js_src', 'source', 'tooltool'])


def directories(pathmodule, cwd, fixup=lambda s: s):
    scripts = pathmodule.join(fixup(cwd), fixup(pathmodule.dirname(__file__)))
    js_src = pathmodule.abspath(pathmodule.join(scripts, "..", ".."))
    source = pathmodule.abspath(pathmodule.join(js_src, "..", ".."))
    tooltool = pathmodule.abspath(env.get('TOOLTOOL_CHECKOUT',
                                          pathmodule.join(source, "..")))
    return Dirs(scripts, js_src, source, tooltool)

# Some scripts will be called with sh, which cannot use backslashed
# paths. So for direct subprocess.* invocation, use normal paths from
# DIR, but when running under the shell, use POSIX style paths.
DIR = directories(os.path, os.getcwd())
PDIR = directories(posixpath, os.environ["PWD"],
                   fixup=lambda s: re.sub(r'^(\w):', r'/\1', s))

parser = argparse.ArgumentParser(
    description='Run a spidermonkey shell build job')
parser.add_argument('--dep', action='store_true',
                    help='do not clobber the objdir before building')
parser.add_argument('--platform', '-p', type=str, metavar='PLATFORM',
                    default='', help='build platform')
parser.add_argument('--timeout', '-t', type=int, metavar='TIMEOUT',
                    default=10800,
                    help='kill job after TIMEOUT seconds')
parser.add_argument('--objdir', type=str, metavar='DIR',
                    default=env.get('OBJDIR', 'obj-spider'),
                    help='object directory')
parser.add_argument('--run-tests', '--tests', type=str, metavar='TESTSUITE',
                    default='',
                    help="comma-separated set of test suites to add to the variant's default set")
parser.add_argument('--skip-tests', '--skip', type=str, metavar='TESTSUITE',
                    default='',
                    help="comma-separated set of test suites to remove from the variant's default set")
parser.add_argument('--build-only', '--build',
                    dest='skip_tests', action='store_const', const='all',
                    help="only do a build, do not run any tests")
parser.add_argument('--nobuild', action='store_true',
                    help='Do not do a build. Rerun tests on existing build.')
parser.add_argument('variant', type=str,
                    help='type of job requested, see variants/ subdir')
args = parser.parse_args()


def set_vars_from_script(script, vars):
    '''Run a shell script, then dump out chosen environment variables. The build
       system uses shell scripts to do some configuration that we need to
       borrow. On Windows, the script itself must output the variable settings
       (in the form "export FOO=<value>"), since otherwise there will be
       problems with mismatched Windows/POSIX formats.
    '''
    script_text = 'source %s' % script
    if platform.system() == 'Windows':
        parse_state = 'parsing exports'
    else:
        script_text += '; echo VAR SETTINGS:; '
        script_text += '; '.join('echo $' + var for var in vars)
        parse_state = 'scanning'
    stdout = subprocess.check_output(['sh', '-x', '-c', script_text])
    tograb = vars[:]
    originals = {}
    for line in stdout.splitlines():
        if parse_state == 'scanning':
            if line == 'VAR SETTINGS:':
                parse_state = 'grabbing'
        elif parse_state == 'grabbing':
            var = tograb.pop(0)
            env[var] = line
        elif parse_state == 'parsing exports':
            m = re.match(r'export (\w+)=(.*)', line)
            if m:
                var, value = m.groups()
                if var in tograb:
                    env[var] = value
                    print("Setting %s = %s" % (var, value))
                if var.startswith("ORIGINAL_"):
                    originals[var[9:]] = value

    # An added wrinkle: on Windows developer systems, the sourced script will
    # blow away current settings for eg LIBS, to point to the ones that would
    # be installed via automation. So we will append the original settings. (On
    # an automation system, the original settings will be empty or point to
    # nonexistent stuff.)
    if platform.system() == 'Windows':
        for var in vars:
            if var in originals and len(originals[var]) > 0:
                env[var] = "%s;%s" % (env[var], originals[var])


def call_alternates(binaries, *args, **kwargs):
    last_exception = None
    for binary in binaries:
        try:
            # Call through a shell due to Windows portability problems.
            rc = subprocess.call(['sh', '-c', binary], *args, **kwargs)
            if rc == 127:
                raise OSError("command not found: %s" % binary)
            return rc
        except OSError as e:
            # Assume the binary was not found.
            last_exception = e
    raise last_exception


def ensure_dir_exists(name, clobber=True):
    if clobber:
        shutil.rmtree(name, ignore_errors=True)
    try:
        os.mkdir(name)
    except OSError:
        if clobber:
            raise

with open(os.path.join(DIR.scripts, "variants", args.variant)) as fh:
    variant = json.load(fh)

if args.variant == 'nonunified':
    # Rewrite js/src/**/moz.build to replace UNIFIED_SOURCES to SOURCES.
    # Note that this modifies the current checkout.
    for dirpath, dirnames, filenames in os.walk(DIR.js_src):
        if 'moz.build' in filenames:
            subprocess.check_call(['sed', '-i', 's/UNIFIED_SOURCES/SOURCES/',
                                   os.path.join(dirpath, 'moz.build')])

if not args.nobuild:
    autoconfs = ['autoconf-2.13', 'autoconf2.13', 'autoconf213']
    if call_alternates(autoconfs, cwd=DIR.js_src) != 0:
        logging.error('autoconf failed')
        sys.exit(1)

OBJDIR = os.path.join(DIR.source, args.objdir)
OUTDIR = os.path.join(OBJDIR, "out")
POBJDIR = posixpath.join(PDIR.source, args.objdir)
AUTOMATION = env.get('AUTOMATION', False)
MAKE = env.get('MAKE', 'make')
MAKEFLAGS = env.get('MAKEFLAGS', '-j6')
CONFIGURE_ARGS = variant['configure-args']
UNAME_M = subprocess.check_output(['uname', '-m']).strip()

# Any jobs that wish to produce additional output can save them into the upload
# directory if there is such a thing, falling back to OBJDIR.
env.setdefault('MOZ_UPLOAD_DIR', OBJDIR)
ensure_dir_exists(env['MOZ_UPLOAD_DIR'], clobber=False)

# Some of the variants request a particular word size (eg ARM simulators).
word_bits = variant.get('bits')

# On Linux and Windows, we build 32- and 64-bit versions on a 64 bit
# host, so the caller has to specify what is desired.
if word_bits is None and args.platform:
    platform_arch = args.platform.split('-')[0]
    if platform_arch in ('win32', 'linux'):
        word_bits = 32
    elif platform_arch in ('win64', 'linux64'):
        word_bits = 64

# Fall back to the word size of the host.
if word_bits is None:
    word_bits = 64 if UNAME_M == 'x86_64' else 32

if 'compiler' in variant:
    compiler = variant['compiler']
elif platform.system() == 'Darwin':
    compiler = 'clang'
elif platform.system() == 'Windows':
    compiler = 'cl'
else:
    compiler = 'gcc'

cxx = {'clang': 'clang++', 'gcc': 'g++', 'cl': 'cl'}.get(compiler)

compiler_dir = env.get('GCCDIR', os.path.join(DIR.tooltool, compiler))
if os.path.exists(os.path.join(compiler_dir, 'bin', compiler)):
    env.setdefault('CC', os.path.join(compiler_dir, 'bin', compiler))
    env.setdefault('CXX', os.path.join(compiler_dir, 'bin', cxx))
    platlib = 'lib64' if word_bits == 64 else 'lib'
    env.setdefault('LD_LIBRARY_PATH', os.path.join(compiler_dir, platlib))
else:
    env.setdefault('CC', compiler)
    env.setdefault('CXX', cxx)

if platform.system() == 'Darwin':
    set_vars_from_script(os.path.join(DIR.scripts, 'macbuildenv.sh'),
                         ['CC', 'CXX'])
elif platform.system() == 'Windows':
    MAKE = env.get('MAKE', 'mozmake')
    os.environ['SOURCE'] = DIR.source
    if word_bits == 64:
        os.environ['USE_64BIT'] = '1'
    set_vars_from_script(posixpath.join(PDIR.scripts, 'winbuildenv.sh'),
                         ['PATH', 'INCLUDE', 'LIB', 'LIBPATH', 'CC', 'CXX',
                          'WINDOWSSDKDIR'])

# Compiler flags, based on word length
if word_bits == 32:
    if compiler == 'clang':
        env['CC'] = '{CC} -arch i386'.format(**env)
        env['CXX'] = '{CXX} -arch i386'.format(**env)
    elif compiler == 'gcc':
        env['CC'] = '{CC} -m32'.format(**env)
        env['CXX'] = '{CXX} -m32'.format(**env)
        env['AR'] = 'ar'

# Configure flags, based on word length and cross-compilation
if word_bits == 32:
    if platform.system() == 'Windows':
        CONFIGURE_ARGS += ' --target=i686-pc-mingw32 --host=i686-pc-mingw32'
    elif platform.system() == 'Linux':
        if UNAME_M != 'arm':
            CONFIGURE_ARGS += ' --target=i686-pc-linux --host=i686-pc-linux'
else:
    if platform.system() == 'Windows':
        CONFIGURE_ARGS += ' --target=x86_64-pc-mingw32 --host=x86_64-pc-mingw32'

# Timeouts.
ACTIVE_PROCESSES = set()


def killall():
    for proc in ACTIVE_PROCESSES:
        proc.kill()
    ACTIVE_PROCESSES.clear()

timer = Timer(args.timeout, killall)
timer.daemon = True
timer.start()

ensure_dir_exists(OBJDIR, clobber=not args.dep and not args.nobuild)
ensure_dir_exists(OUTDIR)


def run_command(command, check=False, **kwargs):
    proc = Popen(command, cwd=OBJDIR, **kwargs)
    ACTIVE_PROCESSES.add(proc)
    stdout, stderr = None, None
    try:
        stdout, stderr = proc.communicate()
    finally:
        ACTIVE_PROCESSES.discard(proc)
    status = proc.wait()
    if check and status != 0:
        raise subprocess.CalledProcessError(status, command, output=stderr)
    return stdout, stderr, status

# Add in environment variable settings for this variant. Normally used to
# modify the flags passed to the shell or to set the GC zeal mode.
for k, v in variant.get('env', {}).items():
    env[k] = v.format(
        DIR=DIR.scripts,
        TOOLTOOL_CHECKOUT=DIR.tooltool,
        MOZ_UPLOAD_DIR=env['MOZ_UPLOAD_DIR'],
        OUTDIR=OUTDIR,
    )

if not args.nobuild:
    CONFIGURE_ARGS += ' --enable-nspr-build'
    CONFIGURE_ARGS += ' --prefix={OBJDIR}/dist'.format(OBJDIR=POBJDIR)
    run_command(['sh', '-c', posixpath.join(PDIR.js_src, 'configure') + ' ' + CONFIGURE_ARGS], check=True)

    run_command('%s -s -w %s' % (MAKE, MAKEFLAGS), shell=True, check=True)

COMMAND_PREFIX = []
# On Linux, disable ASLR to make shell builds a bit more reproducible.
if subprocess.call("type setarch >/dev/null 2>&1", shell=True) == 0:
    COMMAND_PREFIX.extend(['setarch', UNAME_M, '-R'])


def run_test_command(command, **kwargs):
    _, _, status = run_command(COMMAND_PREFIX + command, check=False, **kwargs)
    return status

test_suites = set(['jstests', 'jittest', 'jsapitests', 'checks'])


def normalize_tests(tests):
    if 'all' in tests:
        return test_suites
    return tests

# Need a platform name to use as a key in variant files.
if args.platform:
    variant_platform = args.platform.split("-")[0]
elif platform.system() == 'Windows':
    variant_platform = 'win64' if word_bits == 64 else 'win32'
elif platform.system() == 'Linux':
    variant_platform = 'linux64' if word_bits == 64 else 'linux'
elif platform.system() == 'Darwin':
    variant_platform = 'macosx64'
else:
    variant_platform = 'other'

# Skip any tests that are not run on this platform (or the 'all' platform).
test_suites -= set(normalize_tests(variant.get('skip-tests', {}).get(variant_platform, [])))
test_suites -= set(normalize_tests(variant.get('skip-tests', {}).get('all', [])))

# Add in additional tests for this platform (or the 'all' platform).
test_suites |= set(normalize_tests(variant.get('extra-tests', {}).get(variant_platform, [])))
test_suites |= set(normalize_tests(variant.get('extra-tests', {}).get('all', [])))

# Now adjust the variant's default test list with command-line arguments.
test_suites |= set(normalize_tests(args.run_tests.split(",")))
test_suites -= set(normalize_tests(args.skip_tests.split(",")))

# Always run all enabled tests, even if earlier ones failed. But return the
# first failed status.
results = []

# 'checks' is a superset of 'check-style'.
if 'checks' in test_suites:
    results.append(run_test_command([MAKE, 'check']))
elif 'check-style' in test_suites:
    results.append(run_test_command([MAKE, 'check-style']))

if 'jittest' in test_suites:
    results.append(run_test_command([MAKE, 'check-jit-test']))
if 'jsapitests' in test_suites:
    jsapi_test_binary = os.path.join(OBJDIR, 'dist', 'bin', 'jsapi-tests')
    results.append(run_test_command([jsapi_test_binary]))
if 'jstests' in test_suites:
    results.append(run_test_command([MAKE, 'check-jstests']))

# FIXME bug 1291449: This would be unnecessary if we could run msan with -mllvm
# -msan-keep-going, but in clang 3.8 it causes a hang during compilation.
if variant.get('ignore-test-failures'):
    print("Ignoring test results %s" % (results,))
    results = [0]

if args.variant in ('tsan', 'msan'):
    files = filter(lambda f: f.startswith("sanitize_log."), os.listdir(OUTDIR))
    fullfiles = [os.path.join(OUTDIR, f) for f in files]

    # Summarize results
    sites = Counter()
    for filename in fullfiles:
        with open(os.path.join(OUTDIR, filename), 'rb') as fh:
            for line in fh:
                m = re.match(r'^SUMMARY: \w+Sanitizer: (?:data race|use-of-uninitialized-value) (.*)',
                             line.strip())
                if m:
                    # Some reports include file:line:column, some just
                    # file:line. Just in case it's nondeterministic, we will
                    # canonicalize to just the line number.
                    site = re.sub(r'^(\S+?:\d+)(:\d+)* ', r'\1 ', m.group(1))
                    sites[site] += 1

    # Write a summary file and display it to stdout.
    summary_filename = os.path.join(env['MOZ_UPLOAD_DIR'], "%s_summary.txt" % args.variant)
    with open(summary_filename, 'wb') as outfh:
        for location, count in sites.most_common():
            print >> outfh, "%d %s" % (count, location)
    print(open(summary_filename, 'rb').read())

    if 'max-errors' in variant:
        print("Found %d errors out of %d allowed" % (len(sites), variant['max-errors']))
        if len(sites) > variant['max-errors']:
            results.append(1)

    # Gather individual results into a tarball. Note that these are
    # distinguished only by pid of the JS process running within each test, so
    # given the 16-bit limitation of pids, it's totally possible that some of
    # these files will be lost due to being overwritten.
    command = ['tar', '-C', OUTDIR, '-zcf',
               os.path.join(env['MOZ_UPLOAD_DIR'], '%s.tar.gz' % args.variant)]
    command += files
    subprocess.call(command)

for st in results:
    if st != 0:
        sys.exit(st)
