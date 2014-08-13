# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import with_statement
import os, subprocess, sys, threading, time
from win32 import procmem

def measure_vsize_threadfunc(proc, output_file):
    """
    Measure the virtual memory usage of |proc| at regular intervals
    until it exits, then print the maximum value and write it to
    |output_file|.  Also, print something to the console every
    half an hour to prevent the build job from getting killed when
    linking a large PGOed binary.
    """
    maxvsize = 0
    idleTime = 0
    while proc.returncode is None:
        maxvsize, vsize = procmem.get_vmsize(proc._handle)
        time.sleep(0.5)
        idleTime += 0.5
        if idleTime > 30 * 60:
          print "Still linking, 30 minutes passed..."
          sys.stdout.flush()
          idleTime = 0
    print "TinderboxPrint: linker max vsize: %d" % maxvsize
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
    if sys.platform != "win32":
        print >>sys.stderr, "link.py is only for use on Windows!"
        sys.exit(1)
    if len(sys.argv) < 3:
        print >>sys.stderr, "Usage: link.py <output filename> <commandline>"
        sys.exit(1)
    sys.exit(measure_link_vsize(sys.argv[1], sys.argv[2:]))
