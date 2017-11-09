.. _pgo:

===========================
Profile Guided Optimization
===========================

:abbr:`PGO (Profile Guided Optimization)` is the process of adding
probes to a compiled binary, running said binary, then using the
run-time information to *recompile* the binary to (hopefully) make it
faster.

How PGO Builds Work
===================

The supported interface for invoking a PGO build is to add ``MOZ_PGO=1`` to
configure flags and then build. e.g. in your mozconfig::

    ac_add_options MOZ_PGO=1

Then::

    $ ./mach build

This is roughly equivalent to::

#. Perform a build with *MOZ_PROFILE_GENERATE=1* and *MOZ_PGO_INSTRUMENTED=1*
#. Package with *MOZ_PGO_INSTRUMENTED=1*
#. Performing a run of the instrumented binaries
#. $ make maybe_clobber_profiledbuild
#. Perform a build with *MOZ_PROFILE_USE=1*

Differences between toolchains
==============================

There are some implementation differences depending on the compiler
toolchain being used.

The *maybe_clobber_profiledbuild* step gets its name because of a
difference. On Windows, this step merely moves some *.pgc* files around.
Using GCC or Clang, it is equivalent to a *make clean*.
