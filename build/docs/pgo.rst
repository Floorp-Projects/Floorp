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

#. Perform a build with *--enable-profile-generate* in $topobjdir/instrumented
#. Perform a run of the instrumented binaries with build/pgo/profileserver.py
#. Perform a build with *--enable-profile-use* in $topobjdir
