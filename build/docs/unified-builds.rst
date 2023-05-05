.. _unified-builds:

==============
Unified Builds
==============

The Firefox build system uses the technique of "unified builds" (or elsewhere
called "`unity builds <https://en.wikipedia.org/wiki/Unity_build>`_") to
improve compilation performance. Rather than compiling source files individually,
groups of files in the same directory are concatenated together, then compiled once
in a single batch.

Unified builds can be configured using the ``UNIFIED_SOURCES`` variable in ``moz.build`` files.

.. _unified_build_compilation_failures:

Why are there unrelated compilation failures when I change files?
=================================================================

Since multiple files are concatenated together in a unified build, it's possible for a change
in one file to cause the compilation of a seemingly unrelated file to fail.
This is usually because source files become implicitly dependent on each other for:

* ``#include`` statements
* ``using namespace ...;`` statements
* Other symbol imports or definitions

One of the more common cases of unexpected failures are when source code files are added or
removed, and the "chunking" is changed. There's a limit on the number of files that are combined
together for a single compilation, so sometimes the addition of a new file will cause another one
to be bumped into a different chunk. If that other chunk doesn't meet the implicit requirements
of the bumped file, there will be a tough-to-debug compilation failure.

Building outside of the unified environment
===========================================

As described above, unified builds can cause source files to implicitly depend on each other, which
not only causes unexpected build failures but also can cause issues when using source-analysis tools.
To combat this, we'll use a "non-unified" build that attempts to perform a build with as many files compiled
individually as possible.

To build in the non unified mode, set the following flag in your ``mozconfig``:

``ac_add_options --disable-unified-build``

Other notes:
============

* Some IDEs (such as VSCode with ``clangd``) build files in standalone mode, so they may show
  more failures than a ``mach build``.
* The amount of files per chunk can be adjusted in ``moz.build`` files with the
  ``FILES_PER_UNIFIED_FILE`` variable. Note that changing the chunk size can introduce
  compilation failures as described :ref:`above<unified_build_compilation_failures>`.
* We are happy to accept patches that fix problematic unified build chunks (such as by adding
  includes or namespace annotations).
