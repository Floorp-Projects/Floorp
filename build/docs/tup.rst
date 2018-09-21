.. _tup:

===========
Tup Backend
===========

The Tup build backend is an alternative to the default Make backend. The `Tup
build system <http://gittup.org/tup/>`_ is designed for fast and correct
incremental builds. A top-level no-op build should be under 2 seconds, and
clobbers should rarely be required. It is currently only available for Linux
Desktop builds -- other platforms like Windows or OSX are planned for the
future.

As part of the mozbuild architecture, the Tup backend shares a significant
portion of frontend (developer-facing) code in the build system. When using the
Tup backend, ``mach build`` is still the entry point to run the build system,
and moz.build files are still used for the build description. Familiar parts of
the build system like configure and generating the build files (the
``Reticulating splines...`` step) are virtually identical in both backends. The
difference is that ``mach`` invokes Tup instead of Make under the hood to do
the actual work of determining what needs to be rebuilt. Tup is able to perform
this work more efficiently by loading only the parts of the DAG that are
required for an incremental build. Additionally, Tup instruments processes to
see what files are read from and written to in order to verify that
dependencies are correct.

For more detailed information on the rationale behind Tup, see the `Build
System Rules and Algorithms
<http://gittup.org/tup/build_system_rules_and_algorithms.pdf>`_ paper.

Installation
============

You'll need to install the Tup executable, as well as the nightly rust/cargo
toolchain::

   cd ~/.mozbuild && mach artifact toolchain --from-build linux64-tup
   rustup install nightly
   rustup default nightly

Configuration
=============

Your mozconfig needs to describe how to find the executable if it's not in your
PATH, and enable the Tup backend::

   export TUP=~/.mozbuild/tup/tup
   ac_add_options --enable-build-backends=Tup

What Works
==========

You should expect a Linux desktop build to generate a working Firefox binary
from a ``mach build``, and be able to run test suites against it (eg:
mochitests, xpcshell, gtest). Top-level incremental builds should be fast
enough to use them during a regular compile/edit/test cycle. If you wish to
stop compilation partway through the build to more quickly iterate on a
particular file, you can expect ``mach build objdir/path/to/file.o`` to
correctly produce all inputs required to build file.o before compiling it. For
example, you don't have to run the build system in various subdirectories to
get generated headers built in the right order.

Currently Unsupported / Future Work
===================================

There are a number of features that you may use in the Make backend that are
currently unsupported for the Tup backend. We plan to add support for these in
the future according to developer demand and build team availability.

* sccache - This is currently under active development to support icecream-like
  functionality, which likely impacts the same parts that would affect Tup's
  dependency checking mechanisms. Note that icecream itself should work with
  Tup.

* Incremental Rust compilation - see `bug 1468527 <https://bugzilla.mozilla.org/show_bug.cgi?id=1468527>`_

* Watchman integration - This will allow Tup to skip the initial ``Scanning
  filesystem...`` step, saving 1-2 seconds of startup overhead.

* More platform support (Windows, OSX, etc.)

* Packaging in automation - This is still quite intertwined with Makefiles

* Tests in automation - Requires packaging

Multiple object directories and object directories outside of the source tree
=============================================================================

Common workflows involving multiple object directories which may be outside of
the source directory are supported by Tup, however there are some things to
consider when using these configurations.

* Using multiple object directories works as expected, however you may find
  pulling and attempting to build will fail due to tup attempting to
  parse a Tupfile in an object directory other than the active object
  directory. As a workaround, activate the object directory containing the
  failing file and run configure before proceeding.
* If the currently active object directory is located outside of the source
  directory, the tup backend will prompt the user to run ``tup init --no-sync``
  in a common ancestor directory of the source directory and object directory.
  If this ancestor contains too many files (it's the home directory, for
  instance), this will slow down tup's initial file scan. Anecdotally multiple
  object directories will only incur marginal scanning overhead, but care
  should be exercised when choosing a directory layout.

Tup builds in automation
========================

Tup builds run on integration branches as ``Btup`` in treeherder. There are
some aspects of the Tup builds that are currently implemented outside of the
make build system, and divergences may cause the ``Btup`` job to fail until
the build is completely integrated. There are two known situations this has
come up.

* Changes to the xpidl/webidl/ipdl build - these are given special treatment
  by the make backend that is re-created in the tup backend. An update to a
  Makefile implementing one of these parts of the build may require a
  corresponding change to ``python/mozbuild/mozbuild/backend/tup.py``
* Build scripts generating new outputs - due to the implementation of Tup,
  outputs from ``build.rs`` that run during the build must be known to Tup
  before the build starts. The current outputs are enumerated in
  ``python/mozbuild/mozbuild/backend/cargo_build_defs.py``. Modifying the set
  of outputs from a build script will require a corresponding update to this
  file.


How to Contribute
=================

At the moment we're looking for early adopters who are developing on the Linux
desktop to try out the Tup backend, and share your experiences with the build
team (see `Contact`_).

 * Are there particular issues or missing features that prevent you from using
   the Tup backend at this time?

 * Do you find that top-level incremental builds are fast enough to use for
   every build invocation?

 * Have you needed to perform a clobber build to fix an issue?

Contact
========

If you have any issues, feel free to file a bug blocking `buildtup
<https://bugzilla.mozilla.org/show_bug.cgi?id=827343>`_, or contact mshal or
chmanchester in #build on IRC.
