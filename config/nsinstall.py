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
from __future__ import absolute_import
from __future__ import print_function
from optparse import OptionParser
import mozfile
import os
import os.path
import six
import sys
import shutil


def _nsinstall_internal(argv):
    usage = "usage: %prog [options] arg1 [arg2 ...] target-directory"
    p = OptionParser(usage=usage)

    p.add_option("-D", action="store_true", help="Create a single directory only")
    p.add_option("-t", action="store_true", help="Preserve time stamp")
    p.add_option("-m", action="store", help="Set mode", metavar="mode")
    p.add_option("-d", action="store_true", help="Create directories in target")
    p.add_option(
        "-R", action="store_true", help="Use relative symbolic links (ignored)"
    )
    p.add_option(
        "-L", action="store", metavar="linkprefix", help="Link prefix (ignored)"
    )
    p.add_option(
        "-X",
        action="append",
        metavar="file",
        help="Ignore a file when installing a directory recursively.",
    )

    # The remaining arguments are not used in our tree, thus they're not
    # implented.
    def BadArg(option, opt, value, parser):
        parser.error("option not supported: {0}".format(opt))

    p.add_option(
        "-C", action="callback", metavar="CWD", callback=BadArg, help="NOT SUPPORTED"
    )
    p.add_option(
        "-o",
        action="callback",
        callback=BadArg,
        help="Set owner (NOT SUPPORTED)",
        metavar="owner",
    )
    p.add_option(
        "-g",
        action="callback",
        callback=BadArg,
        help="Set group (NOT SUPPORTED)",
        metavar="group",
    )

    (options, args) = p.parse_args(argv)

    if options.m:
        # mode is specified
        try:
            options.m = int(options.m, 8)
        except Exception:
            sys.stderr.write("nsinstall: {0} is not a valid mode\n".format(options.m))
            return 1

    # just create one directory?
    def maybe_create_dir(dir, mode, try_again):
        dir = os.path.abspath(dir)
        if os.path.exists(dir):
            if not os.path.isdir(dir):
                print("nsinstall: {0} is not a directory".format(dir), file=sys.stderr)
                return 1
            if mode:
                os.chmod(dir, mode)
            return 0

        try:
            if mode:
                os.makedirs(dir, mode)
            else:
                os.makedirs(dir)
        except Exception as e:
            # We might have hit EEXIST due to a race condition (see bug 463411) -- try again once
            if try_again:
                return maybe_create_dir(dir, mode, False)
            print("nsinstall: failed to create directory {0}: {1}".format(dir, e))
            return 1
        else:
            return 0

    if options.X:
        options.X = [os.path.abspath(path) for path in options.X]

    if options.D:
        return maybe_create_dir(args[0], options.m, True)

    # nsinstall arg1 [...] directory
    if len(args) < 2:
        p.error("not enough arguments")

    def copy_all_entries(entries, target):
        for e in entries:
            e = os.path.abspath(e)
            if options.X and e in options.X:
                continue

            dest = os.path.join(target, os.path.basename(e))
            dest = os.path.abspath(dest)
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
            else:
                if os.path.exists(targetpath):
                    if sys.platform == "win32":
                        mozfile.remove(targetpath)
                    else:
                        os.remove(targetpath)
                if options.t:
                    shutil.copy2(srcpath, targetpath)
                else:
                    shutil.copy(srcpath, targetpath)

    # the last argument is the target directory
    target = args.pop()
    # ensure target directory (importantly, we do not apply a mode to the directory
    # because we want to copy files into it and the mode might be read-only)
    rv = maybe_create_dir(target, None, True)
    if rv != 0:
        return rv

    copy_all_entries(args, target)
    return 0


# nsinstall as a native command is always UTF-8


def nsinstall(argv):
    return _nsinstall_internal([six.ensure_text(arg, "utf-8") for arg in argv])


if __name__ == "__main__":
    sys.exit(_nsinstall_internal(sys.argv[1:]))
