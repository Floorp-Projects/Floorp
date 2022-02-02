Process forking in NSPR
=======================

The threads provided in NetScape Portable Runtime (NSPR) are implemented
using different mechanisms on the various platforms. On some platforms,
NSPR threads directly map one-to-one to the threads provided by the
platform vendor, on other platforms NSPR threads are basically
user-level threads within a single process (with no kernel threads) and
on still others NSPR threads are user-level threads implemented on top
of one or more kernel threads within single address space.

NSPR does not override the fork function and so, when fork is called
from the NSPR thread the results are different on the various platforms.
All the threads present in the parent process may be replicated in the
child process, only the calling thread may be replicated in the child
process or only the calling kernel thread may be replicated.

So, to be consistent across all platforms, it is suggested that when
using fork in a NSPR thread;

#. The exec function should be called in the child process.
#. No NSPR functions should be called in the child process before the
   exec call is made.
