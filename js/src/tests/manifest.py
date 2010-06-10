# Library for JSTest manifests.
#
# This includes classes for representing and parsing JS manifests.

import os, re, sys
from subprocess import *

from tests import TestCase

class XULInfo:
    def __init__(self, abi, os, isdebug):
        self.abi = abi
        self.os = os
        self.isdebug = isdebug

    def as_js(self):
        """Return JS that when executed sets up variables so that JS expression
        predicates on XUL build info evaluate properly."""

        return 'var xulRuntime = { OS: "%s", XPCOMABI: "%s", shell: true }; var isDebugBuild=%s;' % (
            self.os,
            self.abi,
            str(self.isdebug).lower())

    @classmethod
    def create(cls, jsdir):
        """Create a XULInfo based on the current platform's characteristics."""

        # Our strategy is to find the autoconf.mk generated for the build and
        # read the values from there.
        
        # Find config/autoconf.mk.
        dir = jsdir
        while True:
            path = os.path.join(dir, 'config/autoconf.mk')
            if os.path.isfile(path):
                break
            if os.path.dirname(dir) == dir:
                print "Can't find config/autoconf.mk on a directory containing the JS shell (searched from %s)"%jsdir
                sys.exit(1)
            dir = os.path.dirname(dir)

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
                raise Exception("Failed to test XUL condition '%s'"%cond)
            self.cache[cond] = ans
        return ans

class NullXULInfoTester:
    """Can be used to parse manifests without a JS shell."""
    def test(self, cond):
        return False

def parse(filename, xul_tester, reldir = ''):
    ans = []
    comment_re = re.compile(r'#.*')
    dir = os.path.dirname(filename)

    try:
        f = open(filename)
    except IOError:
        print "warning: include file not found: '%s'"%filename
        return ans

    for line in f:
        sline = comment_re.sub('', line)
        parts = sline.split()
        if parts[0] == 'include':
            include_file = parts[1]
            include_reldir = os.path.join(reldir, os.path.dirname(include_file))
            ans += parse(os.path.join(dir, include_file), xul_tester, include_reldir)
        elif parts[0] == 'url-prefix':
            # Doesn't apply to shell tests
            pass
        else:
            script = None
            enable = True
            expect = True
            random = False

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
                elif parts[pos] == 'script':
                    script = parts[pos+1]
                    pos += 2
                else:
                    print 'warning: invalid manifest line element "%s"'%parts[pos]
                    pos += 1

            assert script is not None
            ans.append(TestCase(os.path.join(reldir, script), 
                                enable, expect, random))
    return ans
