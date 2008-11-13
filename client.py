#!/usr/bin/python

NSPR_DIRS = ('nsprpub',)
NSS_DIRS  = ('dbm',
             'security/nss',
             'security/coreconf',
             'security/dbm')

import os
import sys
import datetime
import shutil
from optparse import OptionParser
from build.util import check_call

topsrcdir = os.path.dirname(__file__)
if topsrcdir == '':
    topsrcdir = '.'

def check_call_noisy(cmd, *args, **kwargs):
    print "Executing command:", cmd
    check_call(cmd, *args, **kwargs)

def do_hg_pull(dir, repository, hg):
    fulldir = os.path.join(topsrcdir, dir)
    # clone if the dir doesn't exist, pull if it does
    if not os.path.exists(fulldir):
        fulldir = os.path.join(topsrcdir, dir)
        check_call_noisy([hg, 'clone', repository, fulldir])
    else:
        cmd = [hg, 'pull', '-u', '-R', fulldir]
        if repository is not None:
            cmd.append(repository)
        check_call_noisy(cmd)
    check_call([hg, 'parent', '-R', fulldir,
                '--template=Updated to revision {node}.\n'])

def do_cvs_export(modules, tag, cvsroot, cvs):
    """Check out a CVS directory without CVS metadata, using "export"
    modules is a list of directories to check out, e.g. ['nsprpub']
    """
    for module in modules:
        fullpath = os.path.join(topsrcdir, module)
        if os.path.exists(fullpath):
            print "Removing '%s'" % fullpath
            shutil.rmtree(fullpath)

        (parent, leaf) = os.path.split(module)
        print "CVS export begin: " + datetime.datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S UTC")
        check_call_noisy([cvs, '-d', cvsroot,
                          'export', '-r', tag, '-d', leaf,
                          'mozilla/%s' % module],
                         cwd=os.path.join(topsrcdir, parent))
        print "CVS export end: " + datetime.datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S UTC")

o = OptionParser(usage="client.py [options] update_nspr tagname | update_nss tagname")
o.add_option("--skip-mozilla", dest="skip_mozilla",
             action="store_true", default=False,
             help="Obsolete")

o.add_option("--cvs", dest="cvs", default=os.environ.get('CVS', 'cvs'),
             help="The location of the cvs binary")
o.add_option("--cvsroot", dest="cvsroot",
             default=os.environ.get('CVSROOT', ':pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot'),
             help="The CVSROOT (default: :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot")

try:
    options, args = o.parse_args()
    action = args[0]
except IndexError:
    o.print_help()
    sys.exit(2)

if action in ('checkout', 'co'):
    print >>sys.stderr, "Warning: client.py checkout is obsolete."
    pass
elif action in ('update_nspr'):
    tag, = args[1:]
    do_cvs_export(NSPR_DIRS, tag, options.cvsroot, options.cvs)
elif action in ('update_nss'):
    tag, = args[1:]
    do_cvs_export(NSS_DIRS, tag, options.cvsroot, options.cvs)
else:
    o.print_help()
    sys.exit(2)
