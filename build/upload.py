#!/usr/bin/python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# When run directly, this script expects the following environment variables
# to be set:
# UPLOAD_PATH    : path on that host to put the files in
#
# Files are simply copied to UPLOAD_PATH.
#
# All files to be uploaded should be passed as commandline arguments to this
# script. The script takes one other parameter, --base-path, which you can use
# to indicate that files should be uploaded including their paths relative
# to the base path.

import sys
import os
import shutil
from optparse import OptionParser


def OptionalEnvironmentVariable(v):
    """Return the value of the environment variable named v, or None
    if it's unset (or empty)."""
    if v in os.environ and os.environ[v] != "":
        return os.environ[v]
    return None


def FixupMsysPath(path):
    """MSYS helpfully translates absolute pathnames in environment variables
    and commandline arguments into Windows native paths. This sucks if you're
    trying to pass an absolute path on a remote server. This function attempts
    to un-mangle such paths."""
    if 'OSTYPE' in os.environ and os.environ['OSTYPE'] == 'msys':
        # sort of awful, find out where our shell is (should be in msys/bin)
        # and strip the first part of that path out of the other path
        if 'SHELL' in os.environ:
            sh = os.environ['SHELL']
            msys = sh[:sh.find('/bin')]
            if path.startswith(msys):
                path = path[len(msys):]
    return path


def GetBaseRelativePath(path, local_file, base_path):
    """Given a remote path to upload to, a full path to a local file, and an
    optional full path that is a base path of the local file, construct the
    full remote path to place the file in. If base_path is not None, include
    the relative path from base_path to file."""
    dir = os.path.dirname(local_file)
    # strip base_path + extra slash and make it unixy
    dir = dir[len(base_path) + 1:].replace('\\', '/')
    return path + dir


def CopyFilesLocally(path, files, verbose=False, base_path=None, package=None):
    """Copy each file in the list of files to `path`.  The `base_path` argument is treated
    as it is by UploadFiles."""
    if not path.endswith("/"):
        path += "/"
    if base_path is not None:
        base_path = os.path.abspath(base_path)
    for file in files:
        file = os.path.abspath(file)
        if not os.path.isfile(file):
            raise IOError("File not found: %s" % file)
        # first ensure that path exists remotely
        target_path = GetBaseRelativePath(path, file, base_path)
        if not os.path.exists(target_path):
            os.makedirs(target_path)
        if verbose:
            print("Copying " + file + " to " + target_path)
        shutil.copy(file, target_path)


if __name__ == '__main__':
    path = OptionalEnvironmentVariable('UPLOAD_PATH')

    if sys.platform == 'win32':
        if path is not None:
            path = FixupMsysPath(path)

    parser = OptionParser(usage="usage: %prog [options] <files>")
    parser.add_option("-b", "--base-path",
                      action="store",
                      help="Preserve file paths relative to this path when uploading. "
                      "If unset, all files will be uploaded directly to UPLOAD_PATH.")
    parser.add_option("--package",
                      action="store",
                      help="Name of the main package.")
    (options, args) = parser.parse_args()
    if len(args) < 1:
        print("You must specify at least one file to upload")
        sys.exit(1)

    try:
        CopyFilesLocally(path, args, base_path=options.base_path,
                         package=options.package,
                         verbose=True)
    except IOError as strerror:
        print(strerror)
        sys.exit(1)
