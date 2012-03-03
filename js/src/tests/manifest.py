# Library for JSTest manifests.
#
# This includes classes for representing and parsing JS manifests.

import os, re, sys
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

def parse(filename, xul_tester, reldir = ''):
    ans = []
    comment_re = re.compile(r'#.*')
    dirname = os.path.dirname(filename)

    try:
        f = open(filename)
    except IOError:
        print "warning: include file not found: '%s'"%filename
        return ans

    for line in f:
        sline = comment_re.sub('', line)
        parts = sline.split()
        if len(parts) == 0:
            # line is empty or just a comment, skip
            pass
        elif parts[0] == 'include':
            include_file = parts[1]
            include_reldir = os.path.join(reldir, os.path.dirname(include_file))
            ans += parse(os.path.join(dirname, include_file), xul_tester, include_reldir)
        elif parts[0] == 'url-prefix':
            # Doesn't apply to shell tests
            pass
        else:
            script = None
            enable = True
            expect = True
            random = False
            slow = False
            debugMode = False

            pos = 0
            while pos < len(parts):
                if parts[pos] == 'fails':
                    expect = False
                    pos += 1
                elif parts[pos] == 'skip':
                    expect = enable = False
                    pos += 1
                elif parts[pos] == 'random':
                    random = True
                    pos += 1
                elif parts[pos].startswith('fails-if'):
                    cond = parts[pos][len('fails-if('):-1]
                    if xul_tester.test(cond):
                        expect = False
                    pos += 1
                elif parts[pos].startswith('asserts-if'):
                    # This directive means we may flunk some number of
                    # NS_ASSERTIONs in the browser. For the shell, ignore it.
                    pos += 1
                elif parts[pos].startswith('skip-if'):
                    cond = parts[pos][len('skip-if('):-1]
                    if xul_tester.test(cond):
                        expect = enable = False
                    pos += 1
                elif parts[pos].startswith('random-if'):
                    cond = parts[pos][len('random-if('):-1]
                    if xul_tester.test(cond):
                        random = True
                    pos += 1
                elif parts[pos].startswith('require-or'):
                    cond = parts[pos][len('require-or('):-1]
                    (preconditions, fallback_action) = re.split(",", cond)
                    for precondition in re.split("&&", preconditions):
                        if precondition == 'debugMode':
                            debugMode = True
                        elif precondition == 'true':
                            pass
                        else:
                            if fallback_action == "skip":
                                expect = enable = False
                            elif fallback_action == "fail":
                                expect = False
                            elif fallback_action == "random":
                                random = True
                            else:
                                raise Exception(("Invalid precondition '%s' or fallback " +
                                                 " action '%s'") % (precondition, fallback_action))
                            break
                    pos += 1
                elif parts[pos] == 'script':
                    script = parts[pos+1]
                    pos += 2
                elif parts[pos] == 'slow':
                    slow = True
                    pos += 1
                elif parts[pos] == 'silentfail':
                    # silentfails use tons of memory, and Darwin doesn't support ulimit.
                    if xul_tester.test("xulRuntime.OS == 'Darwin'"):
                        expect = enable = False
                    pos += 1
                else:
                    print 'warning: invalid manifest line element "%s"'%parts[pos]
                    pos += 1

            assert script is not None
            ans.append(TestCase(os.path.join(reldir, script),
                                enable, expect, random, slow, debugMode))
    return ans

def check_manifest(test_list):
    test_set = set([ _.path for _ in test_list ])

    missing = []

    for dirpath, dirnames, filenames in os.walk('.'):
        for filename in filenames:
            if dirpath == '.': continue
            if not filename.endswith('.js'): continue
            if filename in ('browser.js', 'shell.js', 'jsref.js', 'template.js'): continue

            path = os.path.join(dirpath, filename)
            if path.startswith('./'):
                path = path[2:]
            if path not in test_set:
                missing.append(path)

    if missing:
        print "Test files not contained in any manifest:"
        for path in missing:
            print path
    else:
        print 'All test files are listed in manifests'
