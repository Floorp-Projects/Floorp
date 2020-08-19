Debugging Firefox with Valgrind
===============================

+--------------------------------------------------------------------+
| This page is an import from MDN and the contents might be outdated |
+--------------------------------------------------------------------+

This page describes how to use Valgrind (specifically, its Memcheck
tool) to find memory errors.

Supported platforms
-------------------

Valgrind runs desktop Firefox fine on Linux, especially on x86 and
x86-64. Firefox for Android and Firefox OS on ARMv7 should also run,
though perhaps not as smoothly. The other architectures supported by
Valgrind on Linux (AARCH64, PPC{32,64}, MIPS{32,64}, S390X) should also
work, in theory.

MacOS X 10.10 (Yosemite), 64-bit only, works, although it can be a bit
of a rough ride.

-  Expect lower performance and a somewhat higher false positive error
   rate than on Linux.
-  Valgrind's handling of malloc zones on Yosemite is imperfect. Regard
   leak reports with caution.
-  Valgrind has been known to cause kernel panics, for unknown reasons.

Where to get Valgrind
---------------------

Linux: Download `Valgrind <http://valgrind.org/>`__ directly, or use
your distribution's package manager.

MacOSX: `Get Valgrind trunk from
SVN <http://valgrind.org/downloads/repository.html>`__ and build it.
Don't use 3.10.x or any other tarball.

Make sure you have version 3.10.1 or later of Valgrind. Newer versions
tend to have better compatibility with both Firefox's JITs and newer
toolchain components (compiler, libc and linker versions).

Basics
------

Build
~~~~~

Build Firefox with the following options, which maximize speed and
accuracy.

.. code:: language-html

   ac_add_options --disable-jemalloc
   ac_add_options --disable-strip
   ac_add_options --enable-valgrind
   ac_add_options --enable-optimize="-g -O2"

Run
~~~

Note that programs run *much* more slowly under Valgrind than they do
natively. Slow-downs of 20x or 30x aren't unexpected, and it's slower on
Mac than on Linux. Don't try this on an underpowered machine.

Linux
^^^^^

On Linux, run Valgrind with the following options.

::

   --smc-check=all-non-file --vex-iropt-register-updates=allregs-at-mem-access --show-mismatched-frees=no --read-inline-info=yes

The ``--smc-check`` and ``--vex-iropt-register-updates`` options are
necessary to avoid crashes in JIT-generated code.

The ``--show-mismatched-frees`` option is necessary due to inconsistent
inlining of ``new`` and ``delete`` -- i.e. one gets inlined but the
other doesn't -- which lead to false-positive mismatched-free errors.

The ``--read-inline-info`` option improves stack trace readability in
the presence of inlining.

Also, run with the following environment variable set.

::

   G_SLICE=always-malloc

This is necessary to get the Gnome system libraries to use plain
``malloc`` instead of pool allocators.

Mac
^^^

On Mac, run Valgrind with the following options.

::

   --smc-check=all-non-file --vex-iropt-register-updates=allregs-at-mem-access --show-mismatched-frees=no --dsymutil=yes

The ``--dsymutil`` option ensures line number information is present in
stack traces.

Advanced usage
--------------

Shared suppression files
~~~~~~~~~~~~~~~~~~~~~~~~

`/build/valgrind/ <http://mxr.mozilla.org/mozilla-central/source/build/valgrind/>`__
contains the suppression files used by the periodic Valgrind jobs on
Tinderbox. Some of these files are platform-specific.

Running mochitests under Valgrind?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To run a mochitest under Valgrind, use the following command.

::

   ./mach mochitest-plain --debugger="valgrind" --debugger-args="$VALGRIND_OPTIONS" relative/path/to/tests

Where ``$VALGRIND_OPTIONS`` are the options described
`above </en-US/docs/Mozilla/Testing/Valgrind#Run>`__. You might also
need ``--trace-children=yes`` to trace into child processes.

As of December 2014 it is possible to do a complete run of
mochitests-plain on Valgrind in about 8 CPU hours on a Core i4910
(Haswell) machine.  Maximum process size is 5.4G, of which about 80% is
in memory.  Runs of small subsets of mochitests take far less memory.

Bits and pieces
~~~~~~~~~~~~~~~

For un-released Linux distros (Fedora Rawhide, etc.) you'll need to use
a version of Valgrind trunk build, because fixes for the latest gcc and
glibc versions appear there first.  Without them you'll be flooded with
false errors from Memcheck, and have debuginfo reading problems.

On Linux, code compiled by LLVM at high optimisation levels can cause
Memcheck to report false uninitialised value errors. See
`here <https://bugs.kde.org/show_bug.cgi?id=242137#c3>`__ for an easy
workaround. On Mac, Valgrind has this workaround built in.

You can make stack traces easier to read by asking for source file names
to be given relative to the root of your source tree.  Do this by using
``--fullpath-after=`` to specify the rightmost part of the absolute path
that you don't want to see.  For example, if your source tree is rooted
at ``/home/sewardj/MC-20-12-2014``, use ``--fullpath-after=2014/`` to
get path names relative to the source directory.

The ``--track-origins=yes`` slows down Valgrind greatly, so don't use it
unless you are hunting down a specific uninitialised value error. But if
you are hunting down such an error, it's extremely helpful and worth
waiting for.

Additional help
---------------

The `Valgrind Quick Start
Guide <http://www.valgrind.org/docs/manual/quick-start.html>`__ is short
and worth reading. The `User
Manual <http://valgrind.org/docs/manual/manual.html>`__ is also useful.

If Valgrind asserts, crashes, doesn't do what you expect, or otherwise
acts up, first of all read this page and make sure you have both Firefox
and Valgrind correctly configured.  If that's all OK, try using the
`Valgrind trunk from
SVN <http://www.valgrind.org/downloads/repository.html>`__.  Oftentimes
bugs are fixed in the trunk before most users fall across them.  If that
doesn't help, consider `filing a bug
report <http://www.valgrind.org/support/bug_reports.html>`__, and/or
mailing Julian Seward or Nick Nethercote.
