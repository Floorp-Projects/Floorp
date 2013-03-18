# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os, os.path
import subprocess
import sys

CL_INCLUDES_PREFIX = os.environ.get("CL_INCLUDES_PREFIX", "Note: including file:")

def InvokeClWithDependencyGeneration(cmdline):
    target = ""
    # Figure out what the target is
    for arg in cmdline:
        if arg.startswith("-Fo"):
            target = arg[3:]
            break

    if target == None:
        print >>sys.stderr, "No target set" and sys.exit(1)

    # The deps target lives here
    depstarget = os.path.basename(target) + ".pp"

    cmdline += ['-showIncludes']
    cl = subprocess.Popen(cmdline, stdout=subprocess.PIPE)

    deps = set()
    for line in cl.stdout:
        # cl -showIncludes prefixes every header with "Note: including file:"
        # and an indentation corresponding to the depth (which we don't need)
        if line.startswith(CL_INCLUDES_PREFIX):
            dep = line[len(CL_INCLUDES_PREFIX):].strip()
            # We can't handle pathes with spaces properly in mddepend.pl, but
            # we can assume that anything in a path with spaces is a system
            # header and throw it away.
            if dep.find(' ') == -1:
                deps.add(dep)
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

    f = open(depstarget, "w")
    for dep in sorted(deps):
        print >>f, "%s: %s" % (target, dep)

if __name__ == "__main__":
    InvokeClWithDependencyGeneration(sys.argv[1:])
