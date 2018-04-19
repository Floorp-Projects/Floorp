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
#  and one of the following:
# UPLOAD_PATH    : path on that host to put the files in
# UPLOAD_TO_TEMP : upload files to a new temporary directory
#
# If UPLOAD_HOST and UPLOAD_USER are not set, this script will simply write out
# the properties file.
#
# If UPLOAD_HOST is "localhost", then files are simply copied to UPLOAD_PATH.
# In this case, UPLOAD_TO_TEMP and POST_UPLOAD_CMD are not supported, and no
# properties are written out.
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

import sys
import os
import re
import json
import errno
import hashlib
import shutil
from optparse import OptionParser
from subprocess import (
    check_call,
    check_output,
    STDOUT,
    CalledProcessError,
)
import concurrent.futures as futures
import redo


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
    # If we're not on Windows, or if we already have an MSYS path (starting
    # with '/' instead of 'c:' or something), then just return.
    if sys.platform != 'win32' or path.startswith('/'):
        return path
    (drive, path) = os.path.splitdrive(os.path.abspath(path))
    return "/" + drive[0] + path.replace('\\', '/')


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
    # In case of an issue here we don't want to hang on a password prompt.
    cmdline.extend(["-o", "BatchMode=yes"])


def DoSSHCommand(command, user, host, port=None, ssh_key=None):
    """Execute command on user@host using ssh. Optionally use
    port and ssh_key, if provided."""
    cmdline = ["ssh"]
    AppendOptionalArgsToSSHCommandline(cmdline, port, ssh_key)
    cmdline.extend(["%s@%s" % (user, host), command])

    with redo.retrying(check_output, sleeptime=10) as f:
        try:
            output = f(cmdline, stderr=STDOUT).strip()
        except CalledProcessError as e:
            print("failed ssh command output:")
            print('=' * 20)
            print(e.output)
            print('=' * 20)
            raise
        return output

    raise Exception("Command %s returned non-zero exit code" % cmdline)


def DoSCPFile(file, remote_path, user, host, port=None, ssh_key=None,
              log=False):
    """Upload file to user@host:remote_path using scp. Optionally use
    port and ssh_key, if provided."""
    if log:
        print('Uploading %s' % file)
    cmdline = ["scp"]
    AppendOptionalArgsToSSHCommandline(cmdline, port, ssh_key)
    cmdline.extend([WindowsPathToMsysPath(file),
                    "%s@%s:%s" % (user, host, remote_path)])
    with redo.retrying(check_call, sleeptime=10) as f:
        f(cmdline)
        return

    raise Exception("Command %s returned non-zero exit code" % cmdline)


def GetBaseRelativePath(path, local_file, base_path):
    """Given a remote path to upload to, a full path to a local file, and an
    optional full path that is a base path of the local file, construct the
    full remote path to place the file in. If base_path is not None, include
    the relative path from base_path to file."""
    if base_path is None or not local_file.startswith(base_path):
        # Hack to work around OSX uploading the i386 SDK from i386/dist. Both
        # the i386 SDK and x86-64 SDK end up in the same directory this way.
        if base_path.endswith('/x86_64/dist'):
            return GetBaseRelativePath(path, local_file, base_path.replace('/x86_64/', '/i386/'))
        return path
    dir = os.path.dirname(local_file)
    # strip base_path + extra slash and make it unixy
    dir = dir[len(base_path) + 1:].replace('\\', '/')
    return path + dir


def GetFileHashAndSize(filename):
    sha512Hash = 'UNKNOWN'
    size = 'UNKNOWN'

    try:
        # open in binary mode to make sure we get consistent results
        # across all platforms
        with open(filename, "rb") as f:
            shaObj = hashlib.sha512(f.read())
            sha512Hash = shaObj.hexdigest()

        size = os.path.getsize(filename)
    except Exception:
        raise Exception("Unable to get filesize/hash from file: %s" % filename)

    return (sha512Hash, size)


def GetMarProperties(filename):
    if not os.path.exists(filename):
        return {}
    (mar_hash, mar_size) = GetFileHashAndSize(filename)
    return {
        'completeMarFilename': os.path.basename(filename),
        'completeMarSize': mar_size,
        'completeMarHash': mar_hash,
    }


def GetUrlProperties(output, package):
    # let's create a switch case using name-spaces/dict
    # rather than a long if/else with duplicate code
    property_conditions = [
        # key: property name, value: condition
        ('symbolsUrl', lambda m: m.endswith('crashreporter-symbols.zip') or
         m.endswith('crashreporter-symbols-full.zip')),
        ('testsUrl', lambda m: m.endswith(('tests.tar.bz2', 'tests.zip'))),
        ('robocopApkUrl', lambda m: m.endswith('apk') and 'robocop' in m),
        ('jsshellUrl', lambda m: 'jsshell-' in m and m.endswith('.zip')),
        ('completeMarUrl', lambda m: m.endswith('.complete.mar')),
        ('partialMarUrl', lambda m: m.endswith('.mar') and '.partial.' in m),
        ('codeCoverageURL', lambda m: m.endswith('code-coverage-gcno.zip')),
        ('sdkUrl', lambda m: m.endswith(('sdk.tar.bz2', 'sdk.zip'))),
        ('testPackagesUrl', lambda m: m.endswith('test_packages.json')),
        ('packageUrl', lambda m: m.endswith(package)),
    ]
    url_re = re.compile(
        r'''^(https?://.*?\.(?:tar\.bz2|dmg|zip|apk|rpm|mar|tar\.gz|json))$''')
    properties = {}

    try:
        for line in output.splitlines():
            m = url_re.match(line.strip())
            if m:
                m = m.group(1)
                for prop, condition in property_conditions:
                    if condition(m):
                        properties.update({prop: m})
                        break
    except IOError as e:
        if e.errno != errno.ENOENT:
            raise
        properties = {prop: 'UNKNOWN' for prop, condition
                      in property_conditions}
    return properties


def UploadFiles(user, host, path, files, verbose=False, port=None, ssh_key=None, base_path=None,
                upload_to_temp_dir=False, post_upload_command=None, package=None):
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
    if not host or not user:
        return {}
    if (not path and not upload_to_temp_dir) or (path and upload_to_temp_dir):
        print("One (and only one of UPLOAD_PATH or UPLOAD_TO_TEMP must be defined.")
        sys.exit(1)

    if upload_to_temp_dir:
        path = DoSSHCommand("mktemp -d", user, host,
                            port=port, ssh_key=ssh_key)
    if not path.endswith("/"):
        path += "/"
    if base_path is not None:
        base_path = os.path.abspath(base_path)
    remote_files = []
    properties = {}

    def get_remote_path(p):
        return GetBaseRelativePath(path, os.path.abspath(p), base_path)

    try:
        # Do a pass to find remote directories so we don't perform excessive
        # scp calls.
        remote_paths = set()
        for file in files:
            if not os.path.isfile(file):
                raise IOError("File not found: %s" % file)

            remote_paths.add(get_remote_path(file))

        # If we wanted to, we could reduce the remote paths if they are a parent
        # of any entry.
        for p in sorted(remote_paths):
            DoSSHCommand("mkdir -p " + p, user, host,
                         port=port, ssh_key=ssh_key)

        with futures.ThreadPoolExecutor(4) as e:
            fs = []
            # Since we're uploading in parallel, the largest file should take
            # the longest to upload. So start it first.
            for file in sorted(files, key=os.path.getsize, reverse=True):
                remote_path = get_remote_path(file)
                fs.append(e.submit(DoSCPFile, file, remote_path, user, host,
                                   port=port, ssh_key=ssh_key, log=verbose))
                remote_files.append(remote_path + '/' + os.path.basename(file))

            # We need to call result() on the future otherwise exceptions could
            # get swallowed.
            for f in futures.as_completed(fs):
                f.result()

        if post_upload_command is not None:
            if verbose:
                print("Running post-upload command: " + post_upload_command)
            file_list = '"' + '" "'.join(remote_files) + '"'
            output = DoSSHCommand('%s "%s" %s' % (
                post_upload_command, path, file_list), user, host, port=port, ssh_key=ssh_key)
            # We print since mozharness may parse URLs from the output stream.
            print(output)
            properties = GetUrlProperties(output, package)
    finally:
        if upload_to_temp_dir:
            DoSSHCommand("rm -rf %s" % path, user, host, port=port,
                         ssh_key=ssh_key)
    if verbose:
        print("Upload complete")
    return properties


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


def WriteProperties(files, properties_file, url_properties, package):
    properties = url_properties
    for file in files:
        if file.endswith('.complete.mar'):
            properties.update(GetMarProperties(file))
    with open(properties_file, 'w') as outfile:
        properties['packageFilename'] = package
        properties['uploadFiles'] = [os.path.abspath(f) for f in files]
        json.dump(properties, outfile, indent=4)


if __name__ == '__main__':
    host = OptionalEnvironmentVariable('UPLOAD_HOST')
    user = OptionalEnvironmentVariable('UPLOAD_USER')
    path = OptionalEnvironmentVariable('UPLOAD_PATH')
    upload_to_temp_dir = OptionalEnvironmentVariable('UPLOAD_TO_TEMP')
    port = OptionalEnvironmentVariable('UPLOAD_PORT')
    if port is not None:
        port = int(port)
    key = OptionalEnvironmentVariable('UPLOAD_SSH_KEY')
    post_upload_command = OptionalEnvironmentVariable('POST_UPLOAD_CMD')

    if sys.platform == 'win32':
        if path is not None:
            path = FixupMsysPath(path)
        if post_upload_command is not None:
            post_upload_command = FixupMsysPath(post_upload_command)

    parser = OptionParser(usage="usage: %prog [options] <files>")
    parser.add_option("-b", "--base-path",
                      action="store",
                      help="Preserve file paths relative to this path when uploading. "
                      "If unset, all files will be uploaded directly to UPLOAD_PATH.")
    parser.add_option("--properties-file",
                      action="store",
                      help="Path to the properties file to store the upload properties.")
    parser.add_option("--package",
                      action="store",
                      help="Name of the main package.")
    (options, args) = parser.parse_args()
    if len(args) < 1:
        print("You must specify at least one file to upload")
        sys.exit(1)
    if not options.properties_file:
        print("You must specify a --properties-file")
        sys.exit(1)

    if host == "localhost":
        if upload_to_temp_dir:
            print("Cannot use UPLOAD_TO_TEMP with UPLOAD_HOST=localhost")
            sys.exit(1)
        if post_upload_command:
            # POST_UPLOAD_COMMAND is difficult to extract from the mozharness
            # scripts, so just ignore it until it's no longer used anywhere
            print("Ignoring POST_UPLOAD_COMMAND with UPLOAD_HOST=localhost")

    try:
        if host == "localhost":
            CopyFilesLocally(path, args, base_path=options.base_path,
                             package=options.package,
                             verbose=True)
        else:

            url_properties = UploadFiles(user, host, path, args,
                                         base_path=options.base_path, port=port, ssh_key=key,
                                         upload_to_temp_dir=upload_to_temp_dir,
                                         post_upload_command=post_upload_command,
                                         package=options.package, verbose=True)

            WriteProperties(args, options.properties_file,
                            url_properties, options.package)
    except IOError as strerror:
        print(strerror)
        sys.exit(1)
