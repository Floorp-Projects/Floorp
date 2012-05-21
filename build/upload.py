#!/usr/bin/python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# When run directly, this script expects the following environment variables
# to be set:
# UPLOAD_HOST    : host to upload files to
# UPLOAD_USER    : username on that host
# UPLOAD_PATH    : path on that host to put the files in
#
# And will use the following optional environment variables if set:
# UPLOAD_SSH_KEY : path to a ssh private key to use
# UPLOAD_PORT    : port to use for ssh
# POST_UPLOAD_CMD: a commandline to run on the remote host after uploading.
#                  UPLOAD_PATH and the full paths of all files uploaded will
#                  be appended to the commandline.
#
# All files to be uploaded should be passed as commandline arguments to this
# script. The script takes one other parameter, --base-path, which you can use
# to indicate that files should be uploaded including their paths relative
# to the base path.

import sys, os
from optparse import OptionParser
from subprocess import PIPE, Popen, check_call

def RequireEnvironmentVariable(v):
    """Return the value of the environment variable named v, or print
    an error and exit if it's unset (or empty)."""
    if not v in os.environ or os.environ[v] == "":
        print "Error: required environment variable %s not set" % v
        sys.exit(1)
    return os.environ[v]

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

def WindowsPathToMsysPath(path):
    """Translate a Windows pathname to an MSYS pathname.
    Necessary because we call out to ssh/scp, which are MSYS binaries
    and expect MSYS paths."""
    if sys.platform != 'win32':
        return path
    (drive, path) = os.path.splitdrive(os.path.abspath(path))
    return "/" + drive[0] + path.replace('\\','/')

def AppendOptionalArgsToSSHCommandline(cmdline, port, ssh_key):
    """Given optional port and ssh key values, append valid OpenSSH
    commandline arguments to the list cmdline if the values are not None."""
    if port is not None:
        cmdline.append("-P%d" % port)
    if ssh_key is not None:
        # Don't interpret ~ paths - ssh can handle that on its own
        if not ssh_key.startswith('~'):
            ssh_key = WindowsPathToMsysPath(ssh_key)
        cmdline.extend(["-o", "IdentityFile=%s" % ssh_key])

def DoSSHCommand(command, user, host, port=None, ssh_key=None):
    """Execute command on user@host using ssh. Optionally use
    port and ssh_key, if provided."""
    cmdline = ["ssh"]
    AppendOptionalArgsToSSHCommandline(cmdline, port, ssh_key)
    cmdline.extend(["%s@%s" % (user, host), command])
    cmd = Popen(cmdline, stdout=PIPE)
    retcode = cmd.wait()
    if retcode != 0:
        raise Exception("Command %s returned non-zero exit code: %i" % \
          (cmdline, retcode))
    return cmd.stdout.read().strip()

def DoSCPFile(file, remote_path, user, host, port=None, ssh_key=None):
    """Upload file to user@host:remote_path using scp. Optionally use
    port and ssh_key, if provided."""
    cmdline = ["scp"]
    AppendOptionalArgsToSSHCommandline(cmdline, port, ssh_key)
    cmdline.extend([WindowsPathToMsysPath(file),
                    "%s@%s:%s" % (user, host, remote_path)])
    check_call(cmdline)

def GetRemotePath(path, local_file, base_path):
    """Given a remote path to upload to, a full path to a local file, and an
    optional full path that is a base path of the local file, construct the
    full remote path to place the file in. If base_path is not None, include
    the relative path from base_path to file."""
    if base_path is None or not local_file.startswith(base_path):
        return path
    dir = os.path.dirname(local_file)
    # strip base_path + extra slash and make it unixy
    dir = dir[len(base_path)+1:].replace('\\','/')
    return path + dir

def UploadFiles(user, host, path, files, verbose=False, port=None, ssh_key=None, base_path=None, upload_to_temp_dir=False, post_upload_command=None):
    """Upload each file in the list files to user@host:path. Optionally pass
    port and ssh_key to the ssh commands. If base_path is not None, upload
    files including their path relative to base_path. If upload_to_temp_dir is
    True files will be uploaded to a temporary directory on the remote server.
    Generally, you should have a post upload command specified in these cases
    that can move them around to their correct location(s).
    If post_upload_command is not None, execute that command on the remote host
    after uploading all files, passing it the upload path, and the full paths to
    all files uploaded.
    If verbose is True, print status updates while working."""
    if upload_to_temp_dir:
        path = DoSSHCommand("mktemp -d", user, host, port=port, ssh_key=ssh_key)
    if not path.endswith("/"):
        path += "/"
    if base_path is not None:
        base_path = os.path.abspath(base_path)
    remote_files = []
    try:
        for file in files:
            file = os.path.abspath(file)
            if not os.path.isfile(file):
                raise IOError("File not found: %s" % file)
            # first ensure that path exists remotely
            remote_path = GetRemotePath(path, file, base_path)
            DoSSHCommand("mkdir -p " + remote_path, user, host, port=port, ssh_key=ssh_key)
            if verbose:
                print "Uploading " + file
            DoSCPFile(file, remote_path, user, host, port=port, ssh_key=ssh_key)
            remote_files.append(remote_path + '/' + os.path.basename(file))
        if post_upload_command is not None:
            if verbose:
                print "Running post-upload command: " + post_upload_command
            file_list = '"' + '" "'.join(remote_files) + '"'
            DoSSHCommand('%s "%s" %s' % (post_upload_command, path, file_list), user, host, port=port, ssh_key=ssh_key)
    finally:
        if upload_to_temp_dir:
            DoSSHCommand("rm -rf %s" % path, user, host, port=port,
                         ssh_key=ssh_key)
    if verbose:
        print "Upload complete"

if __name__ == '__main__':
    host = RequireEnvironmentVariable('UPLOAD_HOST')
    user = RequireEnvironmentVariable('UPLOAD_USER')
    path = OptionalEnvironmentVariable('UPLOAD_PATH')
    upload_to_temp_dir = OptionalEnvironmentVariable('UPLOAD_TO_TEMP')
    port = OptionalEnvironmentVariable('UPLOAD_PORT')
    if port is not None:
        port = int(port)
    key = OptionalEnvironmentVariable('UPLOAD_SSH_KEY')
    post_upload_command = OptionalEnvironmentVariable('POST_UPLOAD_CMD')
    if (not path and not upload_to_temp_dir) or (path and upload_to_temp_dir):
        print "One (and only one of UPLOAD_PATH or UPLOAD_TO_TEMP must be " + \
              "defined."
        sys.exit(1)
    if sys.platform == 'win32':
        if path is not None:
            path = FixupMsysPath(path)
        if post_upload_command is not None:
            post_upload_command = FixupMsysPath(post_upload_command)

    parser = OptionParser(usage="usage: %prog [options] <files>")
    parser.add_option("-b", "--base-path",
                      action="store", dest="base_path",
                      help="Preserve file paths relative to this path when uploading. If unset, all files will be uploaded directly to UPLOAD_PATH.")
    (options, args) = parser.parse_args()
    if len(args) < 1:
        print "You must specify at least one file to upload"
        sys.exit(1)
    try:
        UploadFiles(user, host, path, args, base_path=options.base_path,
                    port=port, ssh_key=key, upload_to_temp_dir=upload_to_temp_dir,
                    post_upload_command=post_upload_command,
                    verbose=True)
    except IOError, (strerror):
        print strerror
        sys.exit(1)
    except Exception, (err):
        print err
        sys.exit(2)
