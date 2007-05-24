#!/usr/bin/python

NSPR_CO_TAG = 'NSPRPUB_PRE_4_2_CLIENT_BRANCH'
NSS_CO_TAG  = 'NSS_3_11_7_BETA1'

NSPR_DIRS = ('nsprpub',)
NSS_DIRS  = ('dbm',
             'security/nss',
             'security/coreconf',
             'security/dbm')

import os
import sys
from optparse import OptionParser

topsrcdir = os.path.dirname(__file__)
if topsrcdir == '':
    topsrcdir = '.'

try:
    from subprocess import check_call
except ImportError:
    import subprocess
    def check_call(*popenargs, **kwargs):
        retcode = subprocess.call(*popenargs, **kwargs)
        if retcode:
            cmd = kwargs.get("args")
            if cmd is None:
                cmd = popenargs[0]
                raise Exception("Command '%s' returned non-zero exit status %i" % (cmd, retcode))

def do_hg_pull(dir, remote, hgroot, hg):
    fulldir = os.path.join(topsrcdir, dir)
    # clone if the dir doesn't exist, pull if it does
    if not os.path.exists(fulldir):
        fulldir = os.path.join(topsrcdir, dir)
        repository = '%s/%s' % (hgroot, remote)
        check_call([hg, 'clone', repository, fulldir])
    else:
        repository = '%s/%s' % (hgroot, remote)
        cmd = [hg, 'pull', '-u', '-R', fulldir, repository]
        check_call(cmd)

def do_cvs_checkout(modules, tag, cvsroot, cvs):
    """Check out a CVS directory.
    modules is a list of directories to check out, e.g. ['nsprpub']
    """
    for module in modules:
        (parent, leaf) = os.path.split(module)
        check_call([cvs, '-d', cvsroot,
                    'checkout', '-P', '-r', tag, '-d', leaf,
                    'mozilla/%s' % module],
                   cwd=os.path.join(topsrcdir, parent))

o = OptionParser(usage="client.py [options] checkout")
o.add_option("-m", "--mozilla-repo", dest="mozilla_repo",
             default="mozilla-central",
             help="Specify the Mozilla repository to pull from, default 'mozilla-central'")
o.add_option("-t", "--tamarin-repo", dest="tamarin_repo",
             default="tamarin-central",
             help="Specify the Tamarin repository to pull from, default 'tamarin-central'")
o.add_option("--hg", dest="hg", default=os.environ.get('HG', 'hg'),
             help="The location of the hg binary")
o.add_option("--cvs", dest="cvs", default=os.environ.get('CVS', 'cvs'),
             help="The location of the cvs binary")
o.add_option("--hgroot", dest="hgroot", default="ssh://hg.mozilla.org",
             help="The hg root (default: ssh://hg.mozilla.org)")
o.add_option("--cvsroot", dest="cvsroot",
             default=os.environ.get('CVSROOT', ':pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot'),
             help="The CVSROOT (default: :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot")

try:
    (options, (action,)) = o.parse_args()
except ValueError:
    o.print_help()
    sys.exit(2)

if action in ('checkout', 'co'):
    do_cvs_checkout(NSPR_DIRS, NSPR_CO_TAG, options.cvsroot, options.cvs)
    do_cvs_checkout(NSS_DIRS, NSS_CO_TAG, options.cvsroot, options.cvs)
    do_hg_pull('js/tamarin', options.tamarin_repo, options.hgroot, options.hg)
    do_hg_pull('.', options.mozilla_repo, options.hgroot, options.hg)
else:
    o.print_help()
    sys.exit(2)
