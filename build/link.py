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
#   Ted Mielczarek <ted@mielczarek.org>
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

from __future__ import with_statement
import os, platform, subprocess, sys, threading, time
from win32 import procmem

def measure_vsize_threadfunc(proc, output_file):
    """
    Measure the virtual memory usage of |proc| at regular intervals
    until it exits, then print the maximum value and write it to
    |output_file|.
    """
    maxvsize = 0
    while proc.returncode is None:
        maxvsize, vsize = procmem.get_vmsize(proc._handle)
        time.sleep(0.5)
    print "linker max virtual size: %d" % maxvsize
    with open(output_file, "w") as f:
        f.write("%d\n" % maxvsize)

def measure_link_vsize(output_file, args):
    """
    Execute |args|, and measure the maximum virtual memory usage of the process,
    printing it to stdout when finished.
    """
    proc = subprocess.Popen(args)
    t = threading.Thread(target=measure_vsize_threadfunc,
                         args=(proc, output_file))
    t.start()
    # Wait for the linker to finish.
    exitcode = proc.wait()
    # ...and then wait for the background thread to finish.
    t.join()
    return exitcode

if __name__ == "__main__":
    if platform.system() != "Windows":
        print >>sys.stderr, "link.py is only for use on Windows!"
        sys.exit(1)
    if len(sys.argv) < 3:
        print >>sys.stderr, "Usage: link.py <output filename> <commandline>"
        sys.exit(1)
    sys.exit(measure_link_vsize(sys.argv[1], sys.argv[2:]))
