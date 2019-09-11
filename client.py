#!/usr/bin/python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

LIBFFI_DIRS = (('js/ctypes/libffi', 'libffi'),)
HG_EXCLUSIONS = ['.hg', '.hgignore', '.hgtags']

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
    print("Executing command:", cmd)
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
    cvs module, e.g. (('js/ctypes/libffi', 'libffi'),)
    """
    for module_tuple in modules:
        module = module_tuple[0]
        cvs_module = module_tuple[1]
        fullpath = os.path.join(topsrcdir, module)
        if os.path.exists(fullpath):
            print("Removing '%s'" % fullpath)
            shutil.rmtree(fullpath)

        (parent, leaf) = os.path.split(module)
        print("CVS export begin: " + datetime.datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S UTC"))
        check_call_noisy([cvs, '-d', cvsroot,
                          'export', '-r', tag, '-d', leaf, cvs_module],
                         cwd=os.path.join(topsrcdir, parent))
        print("CVS export end: " + datetime.datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S UTC"))

def toggle_trailing_blank_line(depname):
  """If the trailing line is empty, then we'll delete it.
  Otherwise we'll add a blank line."""
  lines = open(depname, "r").readlines()
  if not lines:
      print("unexpected short file", file=sys.stderr)
      return

  if not lines[-1].strip():
      # trailing line is blank, removing it
      open(depname, "wb").writelines(lines[:-1])
  else:
      # adding blank line
      open(depname, "ab").write("\n")

def get_trailing_blank_line_state(depname):
  lines = open(depname, "r").readlines()
  if not lines:
      print("unexpected short file", file=sys.stderr)
      return "no blank line"

  if not lines[-1].strip():
      return "has blank line"
  else:
      return "no blank line"

def update_nspr_or_nss(tag, depfile, destination, hgpath):
  destination = destination.rstrip('/')
  permanent_patch_dir = destination + '/patches'
  temporary_patch_dir = destination + '.patches'
  if os.path.exists(temporary_patch_dir):
    print("please clean up leftover directory " + temporary_patch_dir)
    sys.exit(2)
  warn_if_patch_exists(permanent_patch_dir)
  # protect patch directory from being removed by do_hg_replace
  if os.path.exists(permanent_patch_dir):
    shutil.move(permanent_patch_dir, temporary_patch_dir)
  # now update the destination
  print("reverting to HG version of %s to get its blank line state" % depfile)
  check_call_noisy([options.hg, 'revert', depfile])
  old_state = get_trailing_blank_line_state(depfile)
  print("old state of %s is: %s" % (depfile, old_state))
  do_hg_replace(destination, hgpath, tag, HG_EXCLUSIONS, options.hg)
  new_state = get_trailing_blank_line_state(depfile)
  print("new state of %s is: %s" % (depfile, new_state))
  if old_state == new_state:
    print("toggling blank line in: ", depfile)
    toggle_trailing_blank_line(depfile)
  tag_file = destination + "/TAG-INFO"
  with open(tag_file, 'w') as f:
    f.write(tag)
  # move patch directory back to a subdirectory
  if os.path.exists(temporary_patch_dir):
    shutil.move(temporary_patch_dir, permanent_patch_dir)

def warn_if_patch_exists(path):
  # If the given patch directory exists and contains at least one file,
  # then print warning and wait for the user to acknowledge.
  if os.path.isdir(path) and os.listdir(path):
    print("========================================")
    print("WARNING: At least one patch file exists")
    print("in directory: " + path)
    print("You must manually re-apply all patches")
    print("after this script has completed!")
    print("========================================")
    raw_input("Press Enter to continue...")
    return

o = OptionParser(usage="client.py [options] update_nspr tagname | update_nss tagname | update_libffi tagname")
o.add_option("--skip-mozilla", dest="skip_mozilla",
             action="store_true", default=False,
             help="Obsolete")

o.add_option("--cvs", dest="cvs", default=os.environ.get('CVS', 'cvs'),
             help="The location of the cvs binary")
o.add_option("--cvsroot", dest="cvsroot",
             help="The CVSROOT for libffi (default : %s)" % CVSROOT_LIBFFI)
o.add_option("--hg", dest="hg", default=os.environ.get('HG', 'hg'),
             help="The location of the hg binary")
o.add_option("--repo", dest="repo",
             help="the repo to update from (default: upstream repo)")

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
    depfile = "nsprpub/config/prdepend.h"
    if not options.repo:
        options.repo = 'https://hg.mozilla.org/projects/nspr'
    update_nspr_or_nss(tag, depfile, 'nsprpub', options.repo)
elif action in ('update_nss'):
    tag, = args[1:]
    depfile = "security/nss/coreconf/coreconf.dep"
    if not options.repo:
	    options.repo = 'https://hg.mozilla.org/projects/nss'
    update_nspr_or_nss(tag, depfile, 'security/nss', options.repo)
elif action in ('update_libffi'):
    tag, = args[1:]
    if not options.cvsroot:
        options.cvsroot = CVSROOT_LIBFFI
    do_cvs_export(LIBFFI_DIRS, tag, options.cvsroot, options.cvs)
else:
    o.print_help()
    sys.exit(2)
