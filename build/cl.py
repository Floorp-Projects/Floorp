# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is cl.py: a python wrapper for cl to automatically generate
# dependency information.
#
# The Initial Developer of the Original Code is
#   Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Kyle Huey <me@kylehuey.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

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

    depsdir = os.path.normpath(os.path.join(os.path.dirname(target), ".deps"))
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
