#!/usr/bin/env python
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
import os, re, sys, shutil, glob

class ParentDict(dict):
    def __init__(self, parent, **kwargs):
        self.d = dict(kwargs)
        self.parent = parent

    def __setitem__(self, k, v):
        self.d[k] = v

    def __getitem__(self, k):
        if k in self.d:
            return self.d[k]

        return self.parent[k]

thisdir = os.path.dirname(os.path.abspath(__file__))

pymake = [sys.executable, os.path.join(os.path.dirname(thisdir), 'make.py')]
manifest = os.path.join(thisdir, 'tests.manifest')

o = OptionParser()
o.add_option('-g', '--gmake',
             dest="gmake", default="gmake")
o.add_option('-d', '--tempdir',
             dest="tempdir", default="_mktests")
opts, args = o.parse_args()

if len(args) == 0:
    args = [thisdir]

makefiles = []
for a in args:
    if os.path.isfile(a):
        makefiles.append(a)
    elif os.path.isdir(a):
        makefiles.extend(sorted(glob.glob(os.path.join(a, '*.mk'))))

def runTest(makefile, make, logfile, options):
    """
    Given a makefile path, test it with a given `make` and return
    (pass, message).
    """

    if os.path.exists(opts.tempdir): shutil.rmtree(opts.tempdir)
    os.mkdir(opts.tempdir, 0755)

    logfd = open(logfile, 'w')
    p = Popen(make + options['commandline'], stdout=logfd, stderr=STDOUT, env=options['env'])
    logfd.close()
    retcode = p.wait()

    if retcode != options['returncode']:
        return False, "FAIL (returncode=%i)" % retcode
        
    logfd = open(logfile)
    stdout = logfd.read()
    logfd.close()

    if stdout.find('TEST-FAIL') != -1:
        print stdout
        return False, "FAIL (TEST-FAIL printed)"

    if options['grepfor'] and stdout.find(options['grepfor']) == -1:
        print stdout
        return False, "FAIL (%s not in output)" % options['grepfor']

    if options['returncode'] == 0 and stdout.find('TEST-PASS') == -1:
        print stdout
        return False, 'FAIL (No TEST-PASS printed)'

    if options['returncode'] != 0:
        return True, 'PASS (retcode=%s)' % retcode

    return True, 'PASS'

print "%-30s%-28s%-28s" % ("Test:", "gmake:", "pymake:")

gmakefails = 0
pymakefails = 0

tre = re.compile('^#T (gmake |pymake )?([a-z-]+)(?:: (.*))?$')

for makefile in makefiles:
    # For some reason, MAKEFILE_LIST uses native paths in GNU make on Windows
    # (even in MSYS!) so we pass both TESTPATH and NATIVE_TESTPATH
    cline = ['-C', opts.tempdir, '-f', os.path.abspath(makefile), 'TESTPATH=%s' % thisdir.replace('\\','/'), 'NATIVE_TESTPATH=%s' % thisdir]
    if sys.platform == 'win32':
        #XXX: hack so we can specialize the separator character on windows.
        # we really shouldn't need this, but y'know
        cline += ['__WIN32__=1']

    options = {
        'returncode': 0,
        'grepfor': None,
        'env': dict(os.environ),
        'commandline': cline,
        'pass': True,
        'skip': False,
        }

    gmakeoptions = ParentDict(options)
    pymakeoptions = ParentDict(options)

    dmap = {None: options, 'gmake ': gmakeoptions, 'pymake ': pymakeoptions}

    mdata = open(makefile)
    for line in mdata:
        line = line.strip()
        m = tre.search(line)
        if m is None:
            break

        make, key, data = m.group(1, 2, 3)
        d = dmap[make]
        if data is not None:
            data = eval(data)
        if key == 'commandline':
            assert make is None
            d['commandline'].extend(data)
        elif key == 'returncode':
            d['returncode'] = data
        elif key == 'returncode-on':
            if sys.platform in data:
                d['returncode'] = data[sys.platform]
        elif key == 'environment':
            for k, v in data.iteritems():
                d['env'][k] = v
        elif key == 'grep-for':
            d['grepfor'] = data
        elif key == 'fail':
            d['pass'] = False
        elif key == 'skip':
            d['skip'] = True
        else:
            print >>sys.stderr, "%s: Unexpected #T key: %s" % (makefile, key)
            sys.exit(1)

    mdata.close()

    if gmakeoptions['skip']:
        gmakepass, gmakemsg = True, ''
    else:
        gmakepass, gmakemsg = runTest(makefile, [opts.gmake],
                                      makefile + '.gmakelog', gmakeoptions)

    if gmakeoptions['pass']:
        if not gmakepass:
            gmakefails += 1
    else:
        if gmakepass:
            gmakefails += 1
            gmakemsg = "UNEXPECTED PASS"
        else:
            gmakemsg = "KNOWN FAIL"

    if pymakeoptions['skip']:
        pymakepass, pymakemsg = True, ''
    else:
        pymakepass, pymakemsg = runTest(makefile, pymake,
                                        makefile + '.pymakelog', pymakeoptions)

    if pymakeoptions['pass']:
        if not pymakepass:
            pymakefails += 1
    else:
        if pymakepass:
            pymakefails += 1
            pymakemsg = "UNEXPECTED PASS"
        else:
            pymakemsg = "OK (known fail)"

    print "%-30.30s%-28.28s%-28.28s" % (os.path.basename(makefile),
                                        gmakemsg, pymakemsg)

print
print "Summary:"
print "%-30s%-28s%-28s" % ("", "gmake:", "pymake:")

if gmakefails == 0:
    gmakemsg = 'PASS'
else:
    gmakemsg = 'FAIL (%i failures)' % gmakefails

if pymakefails == 0:
    pymakemsg = 'PASS'
else:
    pymakemsg = 'FAIL (%i failures)' % pymakefails

print "%-30.30s%-28.28s%-28.28s" % ('', gmakemsg, pymakemsg)

shutil.rmtree(opts.tempdir)

if gmakefails or pymakefails:
    sys.exit(1)
