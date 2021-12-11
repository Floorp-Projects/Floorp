.. _gn:

==============================
GN support in the build system
==============================

:abbr:`GN (Generated Ninja)` is a third-party build tool used by chromium and
some related projects that are vendored in mozilla-central. Rather than
requiring ``GN`` to build or writing our own build definitions for these projects,
we have support in the build system for translating GN configuration
files into moz.build files. In most cases these moz.build files will be like any
others in the tree (except that they shouldn't be modified by hand), however
those updating vendored code or building on platforms not supported by
Mozilla automation may need to re-generate these files. This is a two-step
process, described below.

Generating GN configs as JSON
=============================

The first step must take place on a machine with access to the ``GN`` tool, which
is specified in a mozconfig with::

    export GN=<path/to/gn>

With this specified, and the tree configured, run::

    $ ./mach build-backend -b GnConfigGen

This will run ``gn gen`` on projects found in ``GN_DIRS`` specified in moz.build
files, injecting variables from the current build configuration and specifying
the result be written to a JSON file in ``$objdir/gn-output``. The file will
have a name derived from the arguments passed to ``gn gen``, for instance
``x64_False_x64_linux.json``.

If updating upstream sources or vendoring a new project, this step must be
performed for each supported build configuration. If adding support for a
specific configuration, the generated configuration may be added to existing
configs before re-generating the ``moz.build`` files, which should be found under
the ``gn-configs`` directory under the vendored project's top-level directory.

Generating moz.build files from GN JSON configs
===============================================

Once the relevant JSON configs are present under a project's ``gn-configs``
directory, run::

    $ ./mach build-backend -b GnMozbuildWriter

This will combine the configuration files present into a set of moz.build files
that will build the given project. Once the result is verified, the resulting
moz.build files should be checked in and should build like any other part of
mozilla-central.
