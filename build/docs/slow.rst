.. _slow:

============================
Why the Build System is Slow
============================

A common complaint about the build system is that it's slow. There are
many reasons contributing to its slowness. We will attempt to document
them here.

First, it is important to distinguish between a :term:`clobber build`
and an :term:`incremental build`. The reasons for why each are slow can
be different.

The build does a lot of work
============================

It may not be obvious, but the main reason the build system is slow is
because it does a lot of work! The source tree consists of a few
thousand C++ files. On a modern machine, we spend over 120 minutes of CPU
core time compiling files! So, if you are looking for the root cause of
slow clobber builds, look at the sheer volume of C++ files in the tree.

You don't have enough CPU cores and MHz
=======================================

The build should be CPU bound. If the build system maintainers are
optimizing the build system perfectly, every CPU core in your machine
should be 100% saturated during a build. While this isn't currently the
case (keep reading below), generally speaking, the more CPU cores you
have in your machine and the more total MHz in your machine, the better.

**We highly recommend building with no fewer than 4 physical CPU
cores.** Please note the *physical* in this sentence. Hyperthreaded
cores (an Intel Core i7 will report 8 CPU cores but only 4 are physical
for example) only yield at most a 1.25x speedup per core.

We also recommend using the most modern CPU model possible. Haswell
chips deliver much more performance per CPU cycle than say Sandy Bridge
CPUs.

This cause impacts both clobber and incremental builds.

You are building with a slow I/O layer
======================================

The build system can be I/O bound if your I/O layer is slow. Linking
libxul on some platforms and build architectures can perform gigabytes
of I/O.

To minimize the impact of slow I/O on build performance, **we highly
recommend building with an SSD.** Power users with enough memory may opt
to build from a RAM disk. Mechanical disks should be avoided if at all
possible.

Some may dispute the importance of an SSD on build times. It is true
that the beneficial impact of an SSD can be mitigated if your system has
lots of memory and the build files stay in the page cache. However,
operating system memory management is complicated. You don't really have
control over what or when something is evicted from the page cache.
Therefore, unless your machine is a dedicated build machine or you have
more memory than is needed by everything running on your machine,
chances are you'll run into page cache eviction and you I/O layer will
impact build performance. That being said, an SSD certainly doesn't
hurt build times. And, anyone who has used a machine with an SSD will
tell you how great of an investment it is for performance all around the
operating system. On top of that, some automated tests are I/O bound
(like those touching SQLite databases), so an SSD will make tests
faster.

This cause impacts both clobber and incremental builds.

You don't have enough memory
============================

The build system allocates a lot of memory, especially when building
many things in parallel. If you don't have enough free system memory,
the build will cause swap activity, slowing down your system and the
build. Even if you never get to the point of swapping, the build system
performs a lot of I/O and having all accessed files in memory and the
page cache can significantly reduce the influence of the I/O layer on
the build system.

**We recommend building with no less than 8 GB of system memory.** As
always, the more memory you have, the better. For a bare bones machine
doing nothing more than building the source tree, anything more than 16
GB is likely entering the point of diminishing returns.

This cause impacts both clobber and incremental builds.

You are building on Windows
===========================

New processes on Windows are about a magnitude slower to spawn than on
UNIX-y systems such as Linux. This is because Windows has optimized new
threads while the \*NIX platforms typically optimize new processes.
Anyway, the build system spawns thousands of new processes during a
build. Parts of the build that rely on rapid spawning of new processes
are slow on Windows as a result. This is most pronounced when running
*configure*. The configure file is a giant shell script and shell
scripts rely heavily on new processes. This is why configure on Windows
can run over a minute slower on Windows.

Another reason Windows builds are slower is because Windows lacks proper
symlink support. On systems that support symlinks, we can generate a
file into a staging area then symlink it into the final directory very
quickly. On Windows, we have to perform a full file copy. This incurs
much more I/O. And if done poorly, can muck with file modification
times, messing up build dependencies. As of the summer of 2013, the
impact of symlinks is being mitigated through the use
of an :term:`install manifest`.

These issues impact both clobber and incremental builds.

Recursive make traversal is slow
================================

The build system has traditionally been built by employing recursive
make. Recursive make involves make iterating through directories / make
files sequentially and executing each in turn. This is inefficient for
directories containing few targets/tasks because make could be *starved*
for work when processing these directories. Any time make is starved,
the build isn't using all available CPU cycles and the build is slower
as a result.

Work has started in bug 907365 to fix this issue by changing the way
make traverses all the make files.

The impact of slow recursive make traversal is mostly felt on
incremental builds. Traditionally, most of the wall time during a
no-op build is spent in make traversal.

make is inefficient
===================

Compared to modern build backends like Tup or Ninja, make is slow and
inefficient. We can only make make so fast. At some point, we'll hit a
performance plateau and will need to use a different tool to make builds
faster.

Please note that clobber and incremental builds are different. A clobber
build with make will likely be as fast as a clobber build with e.g. Tup.
However, Tup should vastly outperform make when it comes to incremental
builds. Therefore, this issue is mostly seen when performing incremental
builds.

C++ header dependency hell
==========================

Modifying a *.h* file can have significant impact on the build system.
If you modify a *.h* that is used by 1000 C++ files, all of those 1000
C++ files will be recompiled.

Our code base has traditionally been sloppy managing the impact of
changed headers on build performance. Bug 785103 tracks improving the
situation.

This issue mostly impacts the times of an :term:`incremental build`.

A search/indexing service on your machine is running
====================================================

Many operating systems have a background service that automatically
indexes filesystem content to make searching faster. On Windows, you
have the Windows Search Service. On OS X, you have Finder.

These background services sometimes take a keen interest in the files
being produced as part of the build. Since the build system produces
hundreds of megabytes or even a few gigabytes of file data, you can
imagine how much work this is to index! If this work is being performed
while the build is running, your build will be slower.

OS X's Finder is notorious for indexing when the build is running. And,
it has a tendency to suck up a whole CPU core. This can make builds
several minutes slower. If you build with ``mach`` and have the optional
``psutil`` package built (it requires Python development headers - see
:ref:`python` for more) and Finder is running during a build, mach will
print a warning at the end of the build, complete with instructions on
how to fix it.
