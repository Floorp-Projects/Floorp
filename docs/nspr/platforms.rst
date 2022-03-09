NSPR platforms
==============

Build System and Supported Platforms
------------------------------------

NSPR has been implemented on over 20 platforms. A platform may have
more than one implementation strategy of its multi threading and I/O
facilities. This article explains the NSPR build system and how to
specify a particular implementation strategy, compiler or compiler
switches, or the target OS for your build on each platform.

Implementation Strategies
-------------------------

Threads are at the core of NSPR, and the I/O functions are tied to the
multi threading facilities because many I/O functions may block the
calling threads. NSPR has multiple implementation strategies of its
multi threading and I/O functions. The various implementation strategies
can be organized into the hierarchy below:

-  **Classic NSPR** (This is our first code base, hence the term
   "classic NSPR"):

**Local threads only**: All threads are user-level threads implemented
by NSPR.
**Global threads only**: All threads are native threads supplied by the
OS vendor. For example, Solaris UI (Unix International) threads and
Win32 threads.
**Combined**: NSPR multiplexes user-level threads on top of native,
kernel-level threads. This is also called the MxN model. At present,
there are three implementations of the combined model.

-  IRIX: sprocs + NSPR user-level threads
-  Windows NT: Win32 threads + NT fibers
-  **Pthreads-user**: kernel-level pthreads + NSPR user-level threads

**Pthreads**: All threads are pthreads. The relevant code is in
``mozilla/nsprpub/pr/src/pthreads`` (threads and I/O).
Classic NSPR and pthreads have relatively disjoint code bases in the
threads and I/O areas:

-  Classic NSPR: ``mozilla/nsprpub/pr/src/threads/combined`` (threads),
   ``mozilla/nsprpub/pr/src/io`` (I/O)
-  Pthreads: ``mozilla/nsprpub/pr/src/pthreads`` (threads and I/O)

Note that some files under ``mozilla/nsprpub/pr/src/io`` are shared by
both classic NSPR and pthreads. Consult
``mozilla/nsprpub/pr/src/Makefile`` for the definitive list of files
used by each implementation strategy (see the definition of the makefile
variable ``OBJS``).

Compilers
---------

For ease of integration with third-party libraries, which may use native
threads, NSPR uses the native threads whenever possible. As a result,
native compilers are used to build NSPR on most platforms because they
have better debugging support for native threads. The only exception is
Solaris, where both cc and gcc are used.

NSPR Build System
-----------------

NSPR build system is based on GNU make.
We use GNU make 3.74 on Unix, but our makefiles should
work fine under newer versions of GNU make.

Every directory in NSPR has a makefile named ``Makefile``, which
includes the makefile fragments in ``mozilla/nsprpub/config``. NSPR
makefiles implement the common Makefile targets such as
``export``, ``libs``, and ``install``. However, some makefiles targets
are no-op in NSPR because they are not necessary for NSPR.

To build NSPR, change directory to the root of our source tree
``cd mozilla/nsprpub``
and then issue the command
``gmake``
Make will recursively go into all the subdirectories and the right
things will happen.

The table below lists the common NSPR makefile targets.

+-----------------------------------+-----------------------------------+
| ``all``                           | The default target. Same as       |
|                                   | ``export`` ``libs`` ``install``.  |
+-----------------------------------+-----------------------------------+
| ``export``                        | Do a complete build.              |
+-----------------------------------+-----------------------------------+
| ``libs``                          | No-op.                            |
+-----------------------------------+-----------------------------------+
| ``install``                       | No-op.                            |
+-----------------------------------+-----------------------------------+
| ``depend``                        | No-op. **This means that NSPR     |
|                                   | makefiles do not have header file |
|                                   | dependencies.**                   |
+-----------------------------------+-----------------------------------+
| ``clean``                         | Remove ``.o`` files.              |
+-----------------------------------+-----------------------------------+
| ``clobber``                       | Remove ``.o`` files, libraries,   |
|                                   | and executable programs.          |
+-----------------------------------+-----------------------------------+
| ``realclean``                     | Remove all generated files and    |
|                                   | directories.                      |
+-----------------------------------+-----------------------------------+
| ``clobber_all``                   | Same as ``realclean``.            |
+-----------------------------------+-----------------------------------+

The table below lists common makefile variables that one can specify
on the command line to customize a build..

+-----------------------------------+-----------------------------------+
| ``BUILD_OPT``                     | Optimized build (default: debug   |
|                                   | build).                           |
+-----------------------------------+-----------------------------------+
| ``OS_TARGET``                     | Set to the target OS (``WIN95``   |
|                                   | or ``WIN16``) when doing          |
|                                   | cross-compilation on NT (default: |
|                                   | same as the host OS).             |
+-----------------------------------+-----------------------------------+
| ``NS_USE_GCC``                    | Use gcc and g++ (default: native  |
|                                   | compilers).                       |
+-----------------------------------+-----------------------------------+
| ``USE_PTHREADS``                  | Build pthreads version.           |
+-----------------------------------+-----------------------------------+
| ``CLASSIC_NSPR``                  | Build classic NSPR version        |
|                                   | (usually local threads only).     |
+-----------------------------------+-----------------------------------+
| ``PTHREADS_USER``                 | Build pthreads-user version.      |
+-----------------------------------+-----------------------------------+
| ``LOCAL_THREADS_ONLY``            | Build local threads only version. |
+-----------------------------------+-----------------------------------+
| ``USE_DEBUG_RTL``                 | On Win32, compile with ``/MDd``   |
|                                   | in the debug build (default:      |
|                                   | ``/MD``). Optimized build always  |
|                                   | uses ``/MD``.                     |
+-----------------------------------+-----------------------------------+
| ``USE_N32``                       | On IRIX, compile with ``-n32``    |
|                                   | (default: ``-32``).               |
+-----------------------------------+-----------------------------------+
| ``USE_IPV6``                      | Enable IPv6.                      |
+-----------------------------------+-----------------------------------+
| ``MOZILLA_CLIENT``                | Adjust NSPR build system for      |
|                                   | Netscape Client (mozilla).        |
+-----------------------------------+-----------------------------------+
