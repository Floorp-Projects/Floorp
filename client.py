#!/usr/bin/python

NSPR_DIRS = (('nsprpub', 'mozilla/nsprpub'),)
NSS_DIRS  = (('dbm', 'mozilla/dbm'),
             ('security/nss', 'mozilla/security/nss'),
             ('security/coreconf', 'mozilla/security/coreconf'),
             ('security/dbm', 'mozilla/security/dbm'))
NSSCKBI_DIRS = (('security/nss/lib/ckfw/builtins', 'mozilla/security/nss/lib/ckfw/builtins'),)
LIBFFI_DIRS = (('js/ctypes/libffi', 'libffi'),)
WEBIDLPARSER_DIR = 'dom/bindings/parser'
WEBIDLPARSER_REPO = 'https://hg.mozilla.org/users/khuey_mozilla.com/webidl-parser'
WEBIDLPARSER_EXCLUSIONS = ['.hgignore', '.gitignore', '.hg', 'ply']

CVSROOT_MOZILLA = ':pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot'
CVSROOT_LIBFFI = ':pserver:anoncvs@sources.redhat.com:/cvs/libffi'

import os
import sys
import datetime
import shutil
import glob
from optparse import OptionParser
from subprocess import check_call

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
        check_call_noisy([hg, 'clone', repository, fulldir])
    else:
        cmd = [hg, 'pull', '-u', '-R', fulldir]
        if repository is not None:
            cmd.append(repository)
        check_call_noisy(cmd)
    check_call([hg, 'parent', '-R', fulldir,
                '--template=Updated to revision {node}.\n'])

def do_hg_replace(dir, repository, tag, exclusions, hg):
    """
        Replace the contents of dir with the contents of repository, except for
        files matching exclusions.
    """
    fulldir = os.path.join(topsrcdir, dir)
    if os.path.exists(fulldir):
        shutil.rmtree(fulldir)

    assert not os.path.exists(fulldir)
    check_call_noisy([hg, 'clone', '-u', tag, repository, fulldir])

    for thing in exclusions:
        for excluded in glob.iglob(os.path.join(fulldir, thing)):
            if os.path.isdir(excluded):
                shutil.rmtree(excluded)
            else:
                os.remove(excluded)

def do_cvs_export(modules, tag, cvsroot, cvs):
    """Check out a CVS directory without CVS metadata, using "export"
    modules is a list of directories to check out and the corresponding
    cvs module, e.g. (('nsprpub', 'mozilla/nsprpub'))
    """
    for module_tuple in modules:
        module = module_tuple[0]
        cvs_module = module_tuple[1]
        fullpath = os.path.join(topsrcdir, module)
        if os.path.exists(fullpath):
            print "Removing '%s'" % fullpath
            shutil.rmtree(fullpath)

        (parent, leaf) = os.path.split(module)
        print "CVS export begin: " + datetime.datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S UTC")
        check_call_noisy([cvs, '-d', cvsroot,
                          'export', '-r', tag, '-d', leaf, cvs_module],
                         cwd=os.path.join(topsrcdir, parent))
        print "CVS export end: " + datetime.datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S UTC")

o = OptionParser(usage="client.py [options] update_nspr tagname | update_nss tagname | update_libffi tagname | update_webidlparser tagname")
o.add_option("--skip-mozilla", dest="skip_mozilla",
             action="store_true", default=False,
             help="Obsolete")

o.add_option("--cvs", dest="cvs", default=os.environ.get('CVS', 'cvs'),
             help="The location of the cvs binary")
o.add_option("--cvsroot", dest="cvsroot",
             help="The CVSROOT (default for mozilla checkouts: %s)" % CVSROOT_MOZILLA)
o.add_option("--hg", dest="hg", default=os.environ.get('HG', 'hg'),
             help="The location of the hg binary")

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
    if not options.cvsroot:
        options.cvsroot = os.environ.get('CVSROOT', CVSROOT_MOZILLA)
    do_cvs_export(NSPR_DIRS, tag, options.cvsroot, options.cvs)
    print >>file("nsprpub/TAG-INFO", "w"), tag
elif action in ('update_nss'):
    tag, = args[1:]
    if not options.cvsroot:
        options.cvsroot = os.environ.get('CVSROOT', CVSROOT_MOZILLA)
    do_cvs_export(NSS_DIRS, tag, options.cvsroot, options.cvs)
    print >>file("security/nss/TAG-INFO", "w"), tag
    print >>file("security/nss/TAG-INFO-CKBI", "w"), tag
elif action in ('update_nssckbi'):
    tag, = args[1:]
    if not options.cvsroot:
        options.cvsroot = os.environ.get('CVSROOT', CVSROOT_MOZILLA)
    do_cvs_export(NSSCKBI_DIRS, tag, options.cvsroot, options.cvs)
    print >>file("security/nss/TAG-INFO-CKBI", "w"), tag
elif action in ('update_libffi'):
    tag, = args[1:]
    if not options.cvsroot:
        options.cvsroot = CVSROOT_LIBFFI
    do_cvs_export(LIBFFI_DIRS, tag, options.cvsroot, options.cvs)
elif action in ('update_webidlparser'):
    tag, = args[1:]
    do_hg_replace(WEBIDLPARSER_DIR, WEBIDLPARSER_REPO, tag, WEBIDLPARSER_EXCLUSIONS, options.hg)
else:
    o.print_help()
    sys.exit(2)
