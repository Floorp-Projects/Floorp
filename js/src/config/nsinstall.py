# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This is a partial python port of nsinstall.
# It's intended to be used when there's no natively compile nsinstall
# available, and doesn't intend to be fully equivalent.
# Its major use is for l10n repackaging on systems that don't have
# a full build environment set up.
# The basic limitation is, it doesn't even try to link and ignores
# all related options.

from optparse import OptionParser
import os
import os.path
import sys
import shutil

def nsinstall(argv):
  usage = "usage: %prog [options] arg1 [arg2 ...] target-directory"
  p = OptionParser(usage=usage)

  p.add_option('-D', action="store_true",
               help="Create a single directory only")
  p.add_option('-t', action="store_true",
               help="Preserve time stamp")
  p.add_option('-m', action="store",
               help="Set mode", metavar="mode")
  p.add_option('-d', action="store_true",
               help="Create directories in target")
  p.add_option('-R', action="store_true",
               help="Use relative symbolic links (ignored)")
  p.add_option('-l', action="store_true",
               help="Create link (ignored)")
  p.add_option('-L', action="store", metavar="linkprefix",
               help="Link prefix (ignored)")
  p.add_option('-X', action="append", metavar="file",
               help="Ignore a file when installing a directory recursively.")

  # The remaining arguments are not used in our tree, thus they're not
  # implented.
  def BadArg(option, opt, value, parser):
    parser.error('option not supported: %s' % opt)
    
  p.add_option('-C', action="callback", metavar="CWD",
               callback=BadArg,
               help="NOT SUPPORTED")
  p.add_option('-o', action="callback", callback=BadArg,
               help="Set owner (NOT SUPPORTED)", metavar="owner")
  p.add_option('-g', action="callback", callback=BadArg,
               help="Set group (NOT SUPPORTED)", metavar="group")

  (options, args) = p.parse_args(argv)

  if options.m:
    # mode is specified
    try:
      options.m = int(options.m, 8)
    except:
      sys.stderr.write('nsinstall: ' + options.m + ' is not a valid mode\n')
      return 1

  # just create one directory?
  if options.D:
    if len(args) != 1:
      return 1
    if os.path.exists(args[0]):
      if not os.path.isdir(args[0]):
        sys.stderr.write('nsinstall: ' + args[0] + ' is not a directory\n')
        sys.exit(1)
      if options.m:
        os.chmod(args[0], options.m)
      sys.exit()
    if options.m:
      os.makedirs(args[0], options.m)
    else:
      os.makedirs(args[0])
    return 0

  if options.X:
    options.X = [os.path.abspath(p) for p in options.X]

  # nsinstall arg1 [...] directory
  if len(args) < 2:
    p.error('not enough arguments')

  def copy_all_entries(entries, target):
    for e in entries:
      if options.X and os.path.abspath(e) in options.X:
        continue

      dest = os.path.join(target,
                          os.path.basename(os.path.normpath(e)))
      handleTarget(e, dest)
      if options.m:
        os.chmod(dest, options.m)

  # set up handler
  if options.d:
    # we're supposed to create directories
    def handleTarget(srcpath, targetpath):
      # target directory was already created, just use mkdir
      os.mkdir(targetpath)
  else:
    # we're supposed to copy files
    def handleTarget(srcpath, targetpath):
      if os.path.isdir(srcpath):
        if not os.path.exists(targetpath):
          os.mkdir(targetpath)
        entries = [os.path.join(srcpath, e) for e in os.listdir(srcpath)]
        copy_all_entries(entries, targetpath)
        # options.t is not relevant for directories
        if options.m:
          os.chmod(targetpath, options.m)
      elif options.t:
        shutil.copy2(srcpath, targetpath)
      else:
        shutil.copy(srcpath, targetpath)

  # the last argument is the target directory
  target = args.pop()
  # ensure target directory
  if not os.path.isdir(target):
    os.makedirs(target)

  copy_all_entries(args, target)
  return 0

if __name__ == '__main__':
  sys.exit(nsinstall(sys.argv[1:]))
