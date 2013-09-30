.. _build_overview:

=====================
Build System Overview
=====================

This document provides an overview on how the build system works. It is
targeted at people wanting to learn about internals of the build system.
It is not meant for persons who casually interact with the build system.
That being said, knowledge empowers, so consider reading on.

The build system is composed of many different components working in
harmony to build the source tree. We begin with a graphic overview.

.. graphviz::

   digraph build_components {
      rankdir="LR";
      "configure" -> "config.status" -> "build backend" -> "build output"
   }

Phase 1: Configuration
======================

Phase 1 centers around the ``configure`` script, which is a bash shell script.
The file is generated from a file called ``configure.in`` which is written in M4
and processed using Autoconf 2.13 to create the final configure script.
You don't have to worry about how you obtain a ``configure`` file: the build
system does this for you.

The primary job of ``configure`` is to determine characteristics of the system
and compiler, apply options passed into it, and validate everything looks OK to
build. The primary output of the ``configure`` script is an executable file
in the object directory called ``config.status``. ``configure`` also produces
some additional files (like ``autoconf.mk``). However, the most important file
in terms of architecture is ``config.status``.

The existence of a ``config.status`` file may be familiar to those who have worked
with Autoconf before. However, Mozilla's ``config.status`` is different from almost
any other ``config.status`` you've ever seen: it's written in Python! Instead of
having our ``configure`` script produce a shell script, we have it generating
Python.

Now is as good a time as any to mention that Python is prevalent in our build
system. If we need to write code for the build system, we do it in Python.
That's just how we roll. For more, see :ref:`python`.

``config.status`` contains 2 parts: data structures representing the output of
``configure`` and a command-line interface for preparing/configuring/generating
an appropriate build backend. (A build backend is merely a tool used to build
the tree - like GNU Make or Tup). These data structures essentially describe
the current state of the system and what the existing build configuration looks
like. For example, it defines which compiler to use, how to invoke it, which
application features are enabled, etc. You are encouraged to open up
``config.status`` to have a look for yourself!

Once we have emitted a ``config.status`` file, we pass into the realm of
phase 2.

Phase 2: Build Backend Preparation and the Build Definition
===========================================================

Once ``configure`` has determined what the current build configuration is,
we need to apply this to the source tree so we can actually build.

What essentially happens is the automatically-produced ``config.status`` Python
script is executed as soon as ``configure`` has generated it. ``config.status``
is charged with the task of tell a tool how to build the tree. To do this,
``config.status`` must first scan the build system definition.

The build system definition consists of various ``moz.build`` files in the tree.
There is roughly one ``moz.build`` file per directory or per set of related directories.
Each ``moz.build`` files defines how its part of the build config works. For
example it says *I want these C++ files compiled* or *look for additional
information in these directories.* config.status starts with the ``moz.build``
file from the root directory and then descends into referenced ``moz.build``
files by following ``DIRS`` variables or similar.

As the ``moz.build`` files are read, data structures describing the overall
build system definition are emitted. These data structures are then fed into a
build backend, which then performs actions, such as writing out files to
be read by a build tool. e.g. a ``make`` backend will write a
``Makefile``.

When ``config.status`` runs, you'll see the following output::

   Reticulating splines...
   Finished reading 1096 moz.build files into 1276 descriptors in 2.40s
   Backend executed in 2.39s
   2188 total backend files. 0 created; 1 updated; 2187 unchanged
   Total wall time: 5.03s; CPU time: 3.79s; Efficiency: 75%

What this is saying is that a total of *1096* ``moz.build`` files were read.
Altogether, *1276* data structures describing the build configuration were
derived from them.  It took *2.40s* wall time to just read these files and
produce the data structures.  The *1276* data structures were fed into the
build backend which then determined it had to manage *2188* files derived
from those data structures. Most of them already existed and didn't need
changed. However, *1* was updated as a result of the new configuration.
The whole process took *5.03s*. Although, only *3.79s* was in
CPU time. That likely means we spent roughly *25%* of the time waiting on
I/O.

For more on how ``moz.build`` files work, see :ref:`mozbuild-files`.

Phase 3: Invokation of the Build Backend
========================================

When most people think of the build system, they think of phase 3. This is
where we take all the code in the tree and produce Firefox or whatever
application you are creating. Phase 3 effectively takes whatever was
generated by phase 2 and runs it. Since the dawn of Mozilla, this has been
make consuming Makefiles. However, with the transition to moz.build files,
you may soon see non-Make build backends, such as Tup or Visual Studio.

When building the tree, most of the time is spent in phase 3. This is when
header files are installed, C++ files are compiled, files are preprocessed, etc.
