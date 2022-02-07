.. rubric:: NSPR: Build System and Supported Platforms
   :name: nspr-build-system-and-supported-platforms

   **Note:** This was written in 1998. Platforms and releases have
   changed since then. See the release notes for current data.

| 
| NSPR has been implemented on over 20 platforms. A platform may have
  more than one implementation strategy of its multi threading and I/O
  facilities. This article explains the NSPR build system and how to
  specify a particular implementation strategy, compiler or compiler
  switches, or the target OS for your build on each platform.

.. rubric:: Implementation Strategies
   :name: implementation-strategies

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
.. rubric:: Compilers
   :name: compilers

For ease of integration with third-party libraries, which may use native
threads, NSPR uses the native threads whenever possible. As a result,
native compilers are used to build NSPR on most platforms because they
have better debugging support for native threads. The only exception is
Solaris, where both cc and gcc are used.
.. rubric:: NSPR Build System
   :name: nspr-build-system

On Unix and Win32, NSPR build system is based on GNU make. (GNU make can
be obtained by anonymous ftp from ``ftp://prep.ai.mit.edu/pub/gnu/``.)
On Mac, NSPR uses Metrowork's CodeWarrior Mac project files.
Inside Netscape, we use GNU make 3.74 on Unix, but our makefiles should
work fine under newer versions of GNU make. On Win32, we use an internal
Win32 port of GNU make 3.74 and a homegrown stripped-down version of
Bourne shell called ``shmsdos.exe``. Our own Win32 port of GNU make and
``shmsdos.exe`` are distributed with the NSPR source. Our makefiles also
work under the official Win32 port of GNU make (3.75 and above) and the
Korn shell ``sh.exe`` in MKS Toolkit.

Every directory in NSPR has a makefile named ``Makefile``, which
includes the makefile fragments in ``mozilla/nsprpub/config``. NSPR
makefiles implement the common Netscape makefile targets such as
``export``, ``libs``, and ``install``. However, some makefiles targets
are no-op in NSPR because they are not necessary for NSPR.

| To build NSPR, change directory to the root of our source tree
| ``cd mozilla/nsprpub``
| and then issue the command
| ``gmake``
| Make will recursively go into all the subdirectories and the right
  things will happen.

| The table below lists the common NSPR makefile targets.
| ` <nspbuild.htm>`__ 

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

| The table below lists common makefile variables that one can specify
  on the command line to customize a build..
|  

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

.. rubric:: Platforms
   :name: platforms

Platforms supported by NSPR can be divided into three categories: Unix,
Windows, and Mac. At present, NSPR aims to support Netscape Communicator
5.0 and Server Suitespot 4.0. The platforms on which we build our binary
releases are listed below.
.. rubric:: Unix
   :name: unix

AIX: 4.2.1

BSD/386: 3.0, 2.1

Digital Unix: V4.0B

FreeBSD: 2.2

HP-UX: B.10.10

HP-UX: B.11.00

Irix: 6.2

Linux: 2.0

MkLinux/PPC: 2.0

NCR: 3.0

NEC: 4.2

SCO_SV: 3.2

SINIX: 5.42

Solaris: 2.5.1 sparc, 2.5.1 x86

SunOS 4: 4.1.3_U1

UNIXWARE: 2.1

.. rubric:: Windows
   :name: windows

Windows NT: 3.51, 4.0, Windows 2000

Windows 95, 98, Me

Windows 3.1 (16-bit Windows)

.. rubric:: Mac
   :name: mac
