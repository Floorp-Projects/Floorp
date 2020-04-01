# Library for JSTest manifests.
#
# This includes classes for representing and parsing JS manifests.

from __future__ import print_function

import os
import posixpath
import re
import sys
from subprocess import Popen, PIPE

from .remote import init_device
from .tests import RefTestCase


def split_path_into_dirs(path):
    dirs = [path]

    while True:
        path, tail = os.path.split(path)
        if not tail:
            break
        dirs.append(path)
    return dirs


class XULInfo:
    def __init__(self, abi, os, isdebug):
        self.abi = abi
        self.os = os
        self.isdebug = isdebug
        self.browserIsRemote = False

    def as_js(self):
        """Return JS that when executed sets up variables so that JS expression
        predicates on XUL build info evaluate properly."""

        return ('var xulRuntime = {{ OS: "{}", XPCOMABI: "{}", shell: true }};'
                'var release_or_beta = getBuildConfiguration().release_or_beta;'
                'var isDebugBuild={}; var Android={}; '
                'var browserIsRemote={}'.format(
                    self.os,
                    self.abi,
                    str(self.isdebug).lower(),
                    str(self.os == "Android").lower(),
                    str(self.browserIsRemote).lower()))

    @classmethod
    def create(cls, jsdir):
        """Create a XULInfo based on the current platform's characteristics."""

        # Our strategy is to find the autoconf.mk generated for the build and
        # read the values from there.

        # Find config/autoconf.mk.
        dirs = split_path_into_dirs(os.getcwd()) + split_path_into_dirs(jsdir)

        path = None
        for dir in dirs:
            _path = posixpath.join(dir, 'config', 'autoconf.mk')
            if os.path.isfile(_path):
                path = _path
                break

        if path is None:
            print("Can't find config/autoconf.mk on a directory containing"
                  " the JS shell (searched from {})".format(jsdir))
            sys.exit(1)

        # Read the values.
        val_re = re.compile(r'(TARGET_XPCOM_ABI|OS_TARGET|MOZ_DEBUG)\s*=\s*(.*)')
        kw = {'isdebug': False}
        for line in open(path):
            m = val_re.match(line)
            if m:
                key, val = m.groups()
                val = val.rstrip()
                if key == 'TARGET_XPCOM_ABI':
                    kw['abi'] = val
                if key == 'OS_TARGET':
                    kw['os'] = val
                if key == 'MOZ_DEBUG':
                    kw['isdebug'] = (val == '1')
        return cls(**kw)


class XULInfoTester:
    def __init__(self, xulinfo, options, js_args):
        self.js_prologue = xulinfo.as_js()
        self.js_bin = options.js_shell
        self.js_args = js_args
        # options here are the command line options
        self.options = options
        # Maps JS expr to evaluation result.
        self.cache = {}

        if not self.options.remote:
            return
        self.device = init_device(options)
        self.js_bin = posixpath.join(options.remote_test_root, 'bin', 'js')

    def test(self, cond, options=[]):
        if self.options.remote:
            return self._test_remote(cond, options=options)
        return self._test_local(cond, options=options)

    def _test_remote(self, cond, options=[]):
        from mozdevice import ADBDevice, ADBProcessError

        ans = self.cache.get(cond, None)
        if ans is not None:
            return ans

        env = {
            'LD_LIBRARY_PATH': posixpath.join(self.options.remote_test_root, 'bin'),
        }

        cmd = [
            self.js_bin
        ] + self.js_args + options + [
            # run in safe configuration, since it is hard to debug
            # crashes when running code here. In particular, msan will
            # error out if the jit is active.
            '--no-baseline',
            '--no-blinterp',
            '-e', self.js_prologue,
            '-e', 'print(!!({}))'.format(cond)
        ]
        cmd = ADBDevice._escape_command_line(cmd)
        try:
            # Allow ADBError or ADBTimeoutError to terminate the test run,
            # but handle ADBProcessError in order to support the use of
            # non-zero exit codes in the JavaScript shell tests.
            out = self.device.shell_output(cmd, env=env,
                                           cwd=self.options.remote_test_root,
                                           timeout=None)
            err = ''
        except ADBProcessError as e:
            out = ''
            err = str(e.adb_process.stdout)

        if out == 'true':
            ans = True
        elif out == 'false':
            ans = False
        else:
            raise Exception("Failed to test XUL condition {!r};"
                            " output was {!r}, stderr was {!r}".format(
                                cond, out, err))
        self.cache[cond] = ans
        return ans

    def _test_local(self, cond, options=[]):
        """Test a XUL predicate condition against this local info."""
        ans = self.cache.get(cond, None)
        if ans is None:
            cmd = [
                self.js_bin
            ] + self.js_args + options + [
                # run in safe configuration, since it is hard to debug
                # crashes when running code here. In particular, msan will
                # error out if the jit is active.
                '--no-baseline',
                '--no-blinterp',
                '-e', self.js_prologue,
                '-e', 'print(!!({}))'.format(cond)
            ]
            p = Popen(cmd, stdin=PIPE, stdout=PIPE, stderr=PIPE, universal_newlines=True)
            out, err = p.communicate()
            if out in ('true\n', 'true\r\n'):
                ans = True
            elif out in ('false\n', 'false\r\n'):
                ans = False
            else:
                raise Exception("Failed to test XUL condition {!r};"
                                " output was {!r}, stderr was {!r}".format(
                                    cond, out, err))
            self.cache[cond] = ans
        return ans


class NullXULInfoTester:
    """Can be used to parse manifests without a JS shell."""

    def test(self, cond, options=[]):
        return False


def _parse_one(testcase, terms, xul_tester):
    pos = 0
    parts = terms.split()
    while pos < len(parts):
        if parts[pos] == 'fails':
            testcase.expect = False
            pos += 1
        elif parts[pos] == 'skip':
            testcase.expect = testcase.enable = False
            pos += 1
        elif parts[pos] == 'random':
            testcase.random = True
            pos += 1
        elif parts[pos].startswith('shell-option('):
            # This directive adds an extra option to pass to the shell.
            option = parts[pos][len('shell-option('):-1]
            testcase.options.append(option)
            pos += 1
        elif parts[pos].startswith('fails-if'):
            cond = parts[pos][len('fails-if('):-1]
            if xul_tester.test(cond, testcase.options):
                testcase.expect = False
            pos += 1
        elif parts[pos].startswith('asserts-if'):
            # This directive means we may flunk some number of
            # NS_ASSERTIONs in the browser. For the shell, ignore it.
            pos += 1
        elif parts[pos].startswith('skip-if'):
            cond = parts[pos][len('skip-if('):-1]
            if xul_tester.test(cond, testcase.options):
                testcase.expect = testcase.enable = False
            pos += 1
        elif parts[pos].startswith('ignore-flag'):
            flag = parts[pos][len('ignore-flag('):-1]
            testcase.ignoredflags.append(flag)
            pos += 1
        elif parts[pos].startswith('random-if'):
            cond = parts[pos][len('random-if('):-1]
            if xul_tester.test(cond, testcase.options):
                testcase.random = True
            pos += 1
        elif parts[pos] == 'slow':
            testcase.slow = True
            pos += 1
        elif parts[pos].startswith('slow-if'):
            cond = parts[pos][len('slow-if('):-1]
            if xul_tester.test(cond, testcase.options):
                testcase.slow = True
            pos += 1
        elif parts[pos] == 'silentfail':
            # silentfails use tons of memory, and Darwin doesn't support ulimit.
            if xul_tester.test("xulRuntime.OS == 'Darwin'", testcase.options):
                testcase.expect = testcase.enable = False
            pos += 1
        elif parts[pos].startswith('error:'):
            # This directive allows to specify an error type.
            (_, _, errortype) = parts[pos].partition(':')
            testcase.error = errortype
            pos += 1
        elif parts[pos] == 'module':
            # This directive marks the test as module code.
            testcase.is_module = True
            pos += 1
        elif parts[pos] == 'async':
            # This directive marks the test as async.
            testcase.is_async = True
            pos += 1
        else:
            print('warning: invalid manifest line element "{}"'.format(
                parts[pos]))
            pos += 1


def _build_manifest_script_entry(script_name, test):
    line = []
    properties = []
    if test.terms:
        # Remove jsreftest internal terms.
        terms = " ".join([term for term in test.terms.split()
                          if not (term == "module" or
                                  term == "async" or
                                  term.startswith("error:") or
                                  term.startswith("ignore-flag(") or
                                  term.startswith("shell-option("))])
        if terms:
            line.append(terms)
    if test.error:
        properties.append("error=" + test.error)
    if test.is_module:
        properties.append("module")
    if test.is_async:
        properties.append("async")
    line.append("script")
    script = script_name
    if properties:
        script = ";".join([script] + properties)
    line.append(script)
    if test.comment:
        line.append("#")
        line.append(test.comment)
    return ' '.join(line)


def _map_prefixes_left(test_gen):
    """
    Splits tests into a dictionary keyed on the first component of the test
    path, aggregating tests with a common base path into a list.
    """
    byprefix = {}
    for t in test_gen:
        left, sep, remainder = t.path.partition(os.sep)
        if left not in byprefix:
            byprefix[left] = []
        if remainder:
            t.path = remainder
        byprefix[left].append(t)
    return byprefix


def _emit_manifest_at(location, relative, test_gen, depth):
    """
    location  - str: absolute path where we want to write the manifest
    relative  - str: relative path from topmost manifest directory to current
    test_gen  - (str): generator of all test paths and directorys
    depth     - int: number of dirs we are below the topmost manifest dir
    """
    manifests = _map_prefixes_left(test_gen)

    filename = os.path.join(location, 'jstests.list')
    manifest = []
    numTestFiles = 0
    for k, test_list in manifests.items():
        fullpath = os.path.join(location, k)
        if os.path.isdir(fullpath):
            manifest.append("include " + k + "/jstests.list")
            relpath = os.path.join(relative, k)
            _emit_manifest_at(fullpath, relpath, test_list, depth + 1)
        else:
            numTestFiles += 1
            assert len(test_list) == 1, test_list
            line = _build_manifest_script_entry(k, test_list[0])
            manifest.append(line)

    # Always present our manifest in sorted order.
    manifest.sort()

    # If we have tests, we have to set the url-prefix so reftest can find them.
    if numTestFiles > 0:
        manifest = ["url-prefix {}jsreftest.html?test={}/".format(
            '../' * depth, relative)] + manifest

    fp = open(filename, 'w')
    try:
        fp.write('\n'.join(manifest) + '\n')
    finally:
        fp.close()


def make_manifests(location, test_gen):
    _emit_manifest_at(location, '', test_gen, 0)


def _find_all_js_files(location):
    for root, dirs, files in os.walk(location):
        root = root[len(location) + 1:]
        for fn in files:
            if fn.endswith('.js'):
                yield root, fn


TEST_HEADER_PATTERN_INLINE = re.compile(r'//\s*\|(.*?)\|\s*(.*?)\s*(--\s*(.*))?$')
TEST_HEADER_PATTERN_MULTI = re.compile(r'/\*\s*\|(.*?)\|\s*(.*?)\s*(--\s*(.*))?\*/')


def _append_terms_and_comment(testcase, terms, comment):
    if testcase.terms is None:
        testcase.terms = terms
    else:
        testcase.terms += " " + terms

    if testcase.comment is None:
        testcase.comment = comment
    elif comment:
        testcase.comment += "; " + comment


def _parse_test_header(fullpath, testcase, xul_tester):
    """
    This looks a bit weird.  The reason is that it needs to be efficient, since
    it has to be done on every test
    """
    fp = open(fullpath, 'r')
    try:
        buf = fp.read(512)
    finally:
        fp.close()

    # Bail early if we do not start with a single comment.
    if not buf.startswith("//"):
        return

    # Extract the token.
    buf, _, _ = buf.partition('\n')
    matches = TEST_HEADER_PATTERN_INLINE.match(buf)

    if not matches:
        matches = TEST_HEADER_PATTERN_MULTI.match(buf)
        if not matches:
            return

    testcase.tag = matches.group(1)
    _append_terms_and_comment(testcase, matches.group(2), matches.group(4))
    _parse_one(testcase, matches.group(2), xul_tester)


def _parse_external_manifest(filename, relpath):
    """
    Reads an external manifest file for test suites whose individual test cases
    can't be decorated with reftest comments.
    filename - str: name of the manifest file
    relpath - str: relative path of the directory containing the manifest
                   within the test suite
    """
    if not os.path.exists(filename):
        return []

    entries = []

    with open(filename, 'r') as fp:
        manifest_re = re.compile(r'^\s*(?P<terms>.*)\s+(?P<type>include|script)\s+(?P<path>\S+)$')
        include_re = re.compile(r'^\s*include\s+(?P<path>\S+)$')
        for line in fp:
            line, _, comment = line.partition('#')
            line = line.strip()
            if not line:
                continue
            matches = manifest_re.match(line)
            if not matches:
                matches = include_re.match(line)
                if not matches:
                    print('warning: unrecognized line in jstests.list:'
                          ' {0}'.format(line))
                    continue

                include_file = matches.group('path')
                include_filename = os.path.join(os.path.dirname(filename), include_file)
                include_relpath = os.path.join(relpath, os.path.dirname(include_file))
                include_entries = _parse_external_manifest(include_filename, include_relpath)
                entries.extend(include_entries)
                continue

            path = os.path.normpath(os.path.join(relpath, matches.group('path')))
            if matches.group('type') == 'include':
                # The manifest spec wants a reference to another manifest here,
                # but we need just the directory. We do need the trailing
                # separator so we don't accidentally match other paths of which
                # this one is a prefix.
                assert(path.endswith('jstests.list'))
                path = path[:-len('jstests.list')]

            entries.append({'path': path, 'terms': matches.group('terms'),
                            'comment': comment.strip()})

    # if one directory name is a prefix of another, we want the shorter one
    # first
    entries.sort(key=lambda x: x["path"])
    return entries


def _apply_external_manifests(filename, testcase, entries, xul_tester):
    for entry in entries:
        if filename.startswith(entry["path"]):
            # The reftest spec would require combining the terms (failure types)
            # that may already be defined in the test case with the terms
            # specified in entry; for example, a skip overrides a random, which
            # overrides a fails. Since we don't necessarily know yet in which
            # environment the test cases will be run, we'd also have to
            # consider skip-if, random-if, and fails-if with as-yet unresolved
            # conditions.
            # At this point, we use external manifests only for test cases
            # that can't have their own failure type comments, so we simply
            # use the terms for the most specific path.
            _append_terms_and_comment(testcase, entry["terms"], entry["comment"])
            _parse_one(testcase, entry["terms"], xul_tester)


def _is_test_file(path_from_root, basename, filename, path_options):
    # Any file whose basename matches something in this set is ignored.
    EXCLUDED = set(('browser.js', 'shell.js', 'template.js',
                    'user.js', 'js-test-driver-begin.js', 'js-test-driver-end.js'))

    # Skip js files in the root test directory.
    if not path_from_root:
        return False

    # Skip files that we know are not tests.
    if basename in EXCLUDED:
        return False

    if not path_options.should_run(filename):
        return False

    return True


def count_tests(location, path_options):
    count = 0
    for root, basename in _find_all_js_files(location):
        filename = os.path.join(root, basename)
        if _is_test_file(root, basename, filename, path_options):
            count += 1
    return count


def load_reftests(location, path_options, xul_tester):
    """
    Locates all tests by walking the filesystem starting at |location|.
    Uses xul_tester to evaluate any test conditions in the test header.
    Failure type and comment for a test case can come from
    - an external manifest entry for the test case,
    - an external manifest entry for a containing directory,
    - most commonly: the header of the test case itself.
    """
    manifestFile = os.path.join(location, 'jstests.list')
    externalManifestEntries = _parse_external_manifest(manifestFile, '')

    for root, basename in _find_all_js_files(location):
        # Get the full path and relative location of the file.
        filename = os.path.join(root, basename)
        if not _is_test_file(root, basename, filename, path_options):
            continue

        # Skip empty files.
        fullpath = os.path.join(location, filename)

        testcase = RefTestCase(location, filename)
        _apply_external_manifests(filename, testcase, externalManifestEntries,
                                  xul_tester)
        _parse_test_header(fullpath, testcase, xul_tester)
        yield testcase
