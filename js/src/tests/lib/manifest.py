# Library for JSTest manifests.
#
# This includes classes for representing and parsing JS manifests.

import os, os.path, re, sys
from subprocess import *

from tests import TestCase


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

        return ('var xulRuntime = { OS: "%s", XPCOMABI: "%s", shell: true };' +
                'var isDebugBuild=%s; var Android=%s; var browserIsRemote=%s') % (
            self.os,
            self.abi,
            str(self.isdebug).lower(),
            str(self.os == "Android").lower(),
            str(self.browserIsRemote).lower())

    @classmethod
    def create(cls, jsdir):
        """Create a XULInfo based on the current platform's characteristics."""

        # Our strategy is to find the autoconf.mk generated for the build and
        # read the values from there.

        # Find config/autoconf.mk.
        dirs = split_path_into_dirs(os.getcwd()) + split_path_into_dirs(jsdir)

        path = None
        for dir in dirs:
            _path = os.path.join(dir, 'config/autoconf.mk')
            if os.path.isfile(_path):
                path = _path
                break

        if path == None:
            print ("Can't find config/autoconf.mk on a directory containing the JS shell"
                   " (searched from %s)") % jsdir
            sys.exit(1)

        # Read the values.
        val_re = re.compile(r'(TARGET_XPCOM_ABI|OS_TARGET|MOZ_DEBUG)\s*=\s*(.*)')
        kw = {}
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
    def __init__(self, xulinfo, js_bin):
        self.js_prolog = xulinfo.as_js()
        self.js_bin = js_bin
        # Maps JS expr to evaluation result.
        self.cache = {}

    def test(self, cond):
        """Test a XUL predicate condition against this local info."""
        ans = self.cache.get(cond, None)
        if ans is None:
            cmd = [ self.js_bin, '-e', self.js_prolog, '-e', 'print(!!(%s))'%cond ]
            p = Popen(cmd, stdin=PIPE, stdout=PIPE, stderr=PIPE)
            out, err = p.communicate()
            if out in ('true\n', 'true\r\n'):
                ans = True
            elif out in ('false\n', 'false\r\n'):
                ans = False
            else:
                raise Exception(("Failed to test XUL condition %r;"
                                 + " output was %r, stderr was %r")
                                 % (cond, out, err))
            self.cache[cond] = ans
        return ans

class NullXULInfoTester:
    """Can be used to parse manifests without a JS shell."""
    def test(self, cond):
        return False

def _parse_one(testcase, xul_tester):
    pos = 0
    parts = testcase.terms.split()
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
        elif parts[pos].startswith('fails-if'):
            cond = parts[pos][len('fails-if('):-1]
            if xul_tester.test(cond):
                testcase.expect = False
            pos += 1
        elif parts[pos].startswith('asserts-if'):
            # This directive means we may flunk some number of
            # NS_ASSERTIONs in the browser. For the shell, ignore it.
            pos += 1
        elif parts[pos].startswith('skip-if'):
            cond = parts[pos][len('skip-if('):-1]
            if xul_tester.test(cond):
                testcase.expect = testcase.enable = False
            pos += 1
        elif parts[pos].startswith('random-if'):
            cond = parts[pos][len('random-if('):-1]
            if xul_tester.test(cond):
                testcase.random = True
            pos += 1
        elif parts[pos].startswith('require-or'):
            cond = parts[pos][len('require-or('):-1]
            (preconditions, fallback_action) = re.split(",", cond)
            for precondition in re.split("&&", preconditions):
                if precondition == 'debugMode':
                    testcase.options.append('-d')
                elif precondition == 'true':
                    pass
                else:
                    if fallback_action == "skip":
                        testcase.expect = testcase.enable = False
                    elif fallback_action == "fail":
                        testcase.expect = False
                    elif fallback_action == "random":
                        testcase.random = True
                    else:
                        raise Exception(("Invalid precondition '%s' or fallback " +
                                         " action '%s'") % (precondition, fallback_action))
                    break
            pos += 1
        elif parts[pos] == 'slow':
            testcase.slow = True
            pos += 1
        elif parts[pos] == 'silentfail':
            # silentfails use tons of memory, and Darwin doesn't support ulimit.
            if xul_tester.test("xulRuntime.OS == 'Darwin'"):
                testcase.expect = testcase.enable = False
            pos += 1
        elif parts[pos] == 'pref(javascript.options.xml.content,true)':
            testcase.options += ['-e', 'options("allow_xml");']
            pos += 1
        else:
            print 'warning: invalid manifest line element "%s"'%parts[pos]
            pos += 1

def _build_manifest_script_entry(script_name, test):
    line = []
    if test.terms:
        line.append(test.terms)
    line.append("script")
    line.append(script_name)
    if test.comment:
        line.append("#")
        line.append(test.comment)
    return ' '.join(line)

def _map_prefixes_left(test_list):
    """
    Splits tests into a dictionary keyed on the first component of the test
    path, aggregating tests with a common base path into a list.
    """
    byprefix = {}
    for t in test_list:
        left, sep, remainder = t.path.partition(os.sep)
        if left not in byprefix:
            byprefix[left] = []
        if remainder:
            t.path = remainder
        byprefix[left].append(t)
    return byprefix

def _emit_manifest_at(location, relative, test_list, depth):
    """
    location  - str: absolute path where we want to write the manifest
    relative  - str: relative path from topmost manifest directory to current
    test_list - [str]: list of all test paths and directorys
    depth     - int: number of dirs we are below the topmost manifest dir
    """
    manifests = _map_prefixes_left(test_list)

    filename = os.path.join(location, 'jstests.list')
    manifest = []
    numTestFiles = 0
    for k, test_list in manifests.iteritems():
        fullpath = os.path.join(location, k)
        if os.path.isdir(fullpath):
            manifest.append("include " + k + "/jstests.list")
            relpath = os.path.join(relative, k)
            _emit_manifest_at(fullpath, relpath, test_list, depth + 1)
        else:
            numTestFiles += 1
            assert(len(test_list) == 1)
            line = _build_manifest_script_entry(k, test_list[0])
            manifest.append(line)

    # Always present our manifest in sorted order.
    manifest.sort()

    # If we have tests, we have to set the url-prefix so reftest can find them.
    if numTestFiles > 0:
        manifest = (["url-prefix %sjsreftest.html?test=%s/" % ('../' * depth, relative)]
                    + manifest)

    fp = open(filename, 'w')
    try:
        fp.write('\n'.join(manifest) + '\n')
    finally:
        fp.close()

def make_manifests(location, test_list):
    _emit_manifest_at(location, '', test_list, 0)

def _find_all_js_files(base, location):
    for root, dirs, files in os.walk(location):
        root = root[len(base) + 1:]
        for fn in files:
            if fn.endswith('.js'):
                yield root, fn

TEST_HEADER_PATTERN_INLINE = re.compile(r'//\s*\|(.*?)\|\s*(.*?)\s*(--\s*(.*))?$')
TEST_HEADER_PATTERN_MULTI  = re.compile(r'/\*\s*\|(.*?)\|\s*(.*?)\s*(--\s*(.*))?\*/')

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
    testcase.terms = matches.group(2)
    testcase.comment = matches.group(4)

    _parse_one(testcase, xul_tester)

def load(location, xul_tester, reldir = ''):
    """
    Locates all tests by walking the filesystem starting at |location|.
    Uses xul_tester to evaluate any test conditions in the test header.
    """
    # The list of tests that we are collecting.
    tests = []

    # Any file who's basename matches something in this set is ignored.
    EXCLUDED = set(('browser.js', 'shell.js', 'jsref.js', 'template.js',
                    'user.js', 'js-test-driver-begin.js', 'js-test-driver-end.js'))

    for root, basename in _find_all_js_files(location, location):
        # Skip js files in the root test directory.
        if not root:
            continue

        # Skip files that we know are not tests.
        if basename in EXCLUDED:
            continue

        # Get the full path and relative location of the file.
        filename = os.path.join(root, basename)
        fullpath = os.path.join(location, filename)

        # Skip empty files.
        statbuf = os.stat(fullpath)
        if statbuf.st_size == 0:
            continue

        # Parse the test header and load the test.
        testcase = TestCase(os.path.join(reldir, filename))
        _parse_test_header(fullpath, testcase, xul_tester)
        tests.append(testcase)
    return tests

