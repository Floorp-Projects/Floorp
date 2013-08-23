# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import ctypes
import os, os.path
import subprocess
import sys
from mozbuild.makeutil import Makefile

CL_INCLUDES_PREFIX = os.environ.get("CL_INCLUDES_PREFIX", "Note: including file:")

GetShortPathName = ctypes.windll.kernel32.GetShortPathNameW
GetLongPathName = ctypes.windll.kernel32.GetLongPathNameW


# cl.exe likes to print inconsistent paths in the showIncludes output
# (some lowercased, some not, with different directions of slashes),
# and we need the original file case for make/pymake to be happy.
# As this is slow and needs to be called a lot of times, use a cache
# to speed things up.
_normcase_cache = {}

def normcase(path):
    # Get*PathName want paths with backslashes
    path = path.replace('/', os.sep)
    dir = os.path.dirname(path)
    # name is fortunately always going to have the right case,
    # so we can use a cache for the directory part only.
    name = os.path.basename(path)
    if dir in _normcase_cache:
        result = _normcase_cache[dir]
    else:
        path = ctypes.create_unicode_buffer(dir)
        length = GetShortPathName(path, None, 0)
        shortpath = ctypes.create_unicode_buffer(length)
        GetShortPathName(path, shortpath, length)
        length = GetLongPathName(shortpath, None, 0)
        if length > len(path):
            path = ctypes.create_unicode_buffer(length)
        GetLongPathName(shortpath, path, length)
        result = _normcase_cache[dir] = path.value
    return os.path.join(result, name)


def InvokeClWithDependencyGeneration(cmdline):
    target = ""
    # Figure out what the target is
    for arg in cmdline:
        if arg.startswith("-Fo"):
            target = arg[3:]
            break

    if target == None:
        print >>sys.stderr, "No target set" and sys.exit(1)

    # Assume the source file is the last argument
    source = cmdline[-1]
    assert not source.startswith('-')

    # The deps target lives here
    depstarget = os.path.basename(target) + ".pp"

    cmdline += ['-showIncludes']
    cl = subprocess.Popen(cmdline, stdout=subprocess.PIPE)

    mk = Makefile()
    rule = mk.create_rule(target)
    rule.add_dependencies([normcase(source)])
    for line in cl.stdout:
        # cl -showIncludes prefixes every header with "Note: including file:"
        # and an indentation corresponding to the depth (which we don't need)
        if line.startswith(CL_INCLUDES_PREFIX):
            dep = line[len(CL_INCLUDES_PREFIX):].strip()
            # We can't handle pathes with spaces properly in mddepend.pl, but
            # we can assume that anything in a path with spaces is a system
            # header and throw it away.
            if ' ' not in dep:
                rule.add_dependencies([normcase(dep)])
        else:
            sys.stdout.write(line) # Make sure we preserve the relevant output
                                   # from cl

    ret = cl.wait()
    if ret != 0 or target == "":
        sys.exit(ret)

    depsdir = os.path.normpath(os.path.join(os.curdir, ".deps"))
    depstarget = os.path.join(depsdir, depstarget)
    if not os.path.isdir(depsdir):
        try:
            os.makedirs(depsdir)
        except OSError:
            pass # This suppresses the error we get when the dir exists, at the
                 # cost of masking failure to create the directory.  We'll just
                 # die on the next line though, so it's not that much of a loss.

    with open(depstarget, "w") as f:
        mk.dump(f)

if __name__ == "__main__":
    InvokeClWithDependencyGeneration(sys.argv[1:])
