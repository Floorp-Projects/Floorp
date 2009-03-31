"""
Run the test(s) listed on the command line. If a directory is listed, the script will recursively
walk the directory for files named .mk and run each.

For each test, we run gmake -f test.mk. By default, make must exit with an exit code of 0, and must print 'TEST-PASS'.

Each test is run in an empty directory.

The test file may contain lines at the beginning to alter the default behavior. These are all evaluated as python:

#T commandline: ['extra', 'params', 'here']
#T returncode: 2
#T returncode-on: {'win32': 2}
#T environment: {'VAR': 'VALUE}
#T grep-for: "text"
"""

from subprocess import Popen, PIPE, STDOUT
from optparse import OptionParser
import os, re, sys, shutil

thisdir = os.path.dirname(os.path.abspath(__file__))

o = OptionParser()
o.add_option('-m', '--make',
             dest="make", default="gmake")
o.add_option('-d', '--tempdir',
             dest="tempdir", default="_mktests")
opts, args = o.parse_args()

if len(args) == 0:
    args = ['.']

makefiles = []
for a in args:
    if os.path.isfile(a):
        makefiles.append(a)
    elif os.path.isdir(a):
        for path, dirnames, filenames in os.walk(a):
            for f in filenames:
                if f.endswith('.mk'):
                    makefiles.append('%s/%s' % (path, f))
    else:
        print >>sys.stderr, "Error: Unknown file on command line"
        sys.exit(1)

tre = re.compile('^#T ([a-z-]+): (.*)$')

for makefile in makefiles:
    print "Testing: %s" % makefile,

    if os.path.exists(opts.tempdir): shutil.rmtree(opts.tempdir)
    os.mkdir(opts.tempdir, 0755)

    # For some reason, MAKEFILE_LIST uses native paths in GNU make on Windows
    # (even in MSYS!) so we pass both TESTPATH and NATIVE_TESTPATH
    cline = [opts.make, '-C', opts.tempdir, '-f', os.path.abspath(makefile), 'TESTPATH=%s' % thisdir.replace('\\','/'), 'NATIVE_TESTPATH=%s' % thisdir]
    if sys.platform == 'win32':
        #XXX: hack to run pymake on windows
        if opts.make.endswith('.py'):
            cline = [sys.executable] + cline
        #XXX: hack so we can specialize the separator character on windows.
        # we really shouldn't need this, but y'know
        cline += ['__WIN32__=1']
        
    returncode = 0
    grepfor = None

    env = dict(os.environ)

    mdata = open(makefile)
    for line in mdata:
        m = tre.search(line)
        if m is None:
            break
        key, data = m.group(1, 2)
        data = eval(data)
        if key == 'commandline':
            cline.extend(data)
        elif key == 'returncode':
            returncode = data
        elif key == 'returncode-on':
            if sys.platform in data:
                returncode = data[sys.platform]
        elif key == 'environment':
            for k, v in data.iteritems():
                env[k] = v
        elif key == 'grep-for':
            grepfor = data
        else:
            print >>sys.stderr, "Unexpected #T key: %s" % key
            sys.exit(1)

    mdata.close()

    p = Popen(cline, stdout=PIPE, stderr=STDOUT, env=env)
    stdout, d = p.communicate()
    if p.returncode != returncode:
        print "FAIL (returncode=%i)" % p.returncode
        print stdout
    elif stdout.find('TEST-FAIL') != -1:
        print "FAIL"
        print stdout
    elif returncode == 0:
        if stdout.find(grepfor or 'TEST-PASS') != -1:
            print "PASS"
        else:
            print "FAIL (no expected output)"
            print stdout
    # check that test produced the expected output while failing
    elif grepfor:
        if stdout.find(grepfor) != -1:
            print "PASS"
        else:
            print "FAIL (no expected output)"
            print stdout
    else:
        print "EXPECTED-FAIL"

shutil.rmtree(opts.tempdir)
