#!/usr/bin/python

NSPR_CO_TAG = 'NSPR_HEAD_20071016'
NSS_CO_TAG  = 'NSS_3_12_ALPHA_2'

NSPR_DIRS = ('nsprpub',)
NSS_DIRS  = ('dbm',
             'security/nss',
             'security/coreconf',
             'security/dbm')

# URL of the default hg repository to clone for Tamarin.
DEFAULT_TAMARIN_REPO = 'http://hg.mozilla.org/tamarin-central/'

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

def do_cvs_checkout(modules, tag, cvsroot, cvs):
    """Check out a CVS directory.
    modules is a list of directories to check out, e.g. ['nsprpub']
    """
    for module in modules:
        (parent, leaf) = os.path.split(module)
        check_call_noisy([cvs, '-d', cvsroot,
                          'checkout', '-P', '-r', tag, '-d', leaf,
                          'mozilla/%s' % module],
                         cwd=os.path.join(topsrcdir, parent))

o = OptionParser(usage="client.py [options] checkout")
o.add_option("-m", "--mozilla-repo", dest="mozilla_repo",
             default=None,
             help="URL of Mozilla repository to pull from (default: use hg default in .hg/hgrc)")
o.add_option("-t", "--tamarin-repo", dest="tamarin_repo",
             default=None,
             help="URL of Tamarin repository to pull from (default: use hg default in js/tamarin/.hg/hgrc; or if that file doesn't exist, use \"" + DEFAULT_TAMARIN_REPO + "\".)")
o.add_option("--hg", dest="hg", default=os.environ.get('HG', 'hg'),
             help="The location of the hg binary")
o.add_option("--cvs", dest="cvs", default=os.environ.get('CVS', 'cvs'),
             help="The location of the cvs binary")
o.add_option("--cvsroot", dest="cvsroot",
             default=os.environ.get('CVSROOT', ':pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot'),
             help="The CVSROOT (default: :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot")


def fixup_repo_options(options):
    """ Check options.mozilla_repo and options.tamarin_repo values;
    populate tamarin_repo if needed.

    options.mozilla_repo and options.tamarin_repo are normally None.
    This is fine-- our "hg pull" commands will omit the repo URL.
    The exception is the initial checkout, which does an "hg clone"
    for Tamarin.  That command requires a repository URL.
    """

    if (options.mozilla_repo is None
            and not os.path.exists(os.path.join(topsrcdir, '.hg'))):
        o.print_help()
        print
        print "*** The -m option is required for the initial checkout."
        sys.exit(2)

    # Handle special case: initial checkout of Tamarin.
    if (options.tamarin_repo is None
            and not os.path.exists(os.path.join(topsrcdir, 'js', 'tamarin'))):
        options.tamarin_repo = DEFAULT_TAMARIN_REPO

try:
    (options, (action,)) = o.parse_args()
except ValueError:
    o.print_help()
    sys.exit(2)

fixup_repo_options(options)

if action in ('checkout', 'co'):
    do_cvs_checkout(NSPR_DIRS, NSPR_CO_TAG, options.cvsroot, options.cvs)
    do_cvs_checkout(NSS_DIRS, NSS_CO_TAG, options.cvsroot, options.cvs)
    do_hg_pull('js/tamarin', options.tamarin_repo, options.hg)
    do_hg_pull('.', options.mozilla_repo, options.hg)
else:
    o.print_help()
    sys.exit(2)
