.. _build_targets:

=============
Build Targets
=============

When you build with ``mach build``, there are some special targets that can be
built. This page attempts to document them.

Partial Tree Targets
====================

The targets in this section only build part of the tree. Please note that
partial tree builds can be unreliable. Use at your own risk.

export
   Build the *export* tier. The *export* tier builds everything that is
   required for C/C++ compilation. It stages all header files, processes
   IDLs, etc.

compile
   Build the *compile* tier. The *compile* tier compiles all C/C++ files.
   Only applies to builds with ``MOZ_PSEUDO_DERECURSE``.

libs
   Build the *libs* tier. The *libs* tier performs linking and performs
   most build steps which aren't related to compilation.

tools
   Build the *tools* tier. The *tools* tier mostly deals with supplementary
   tools and compiled tests. It will link tools against libXUL, including
   compiled test binaries.

binaries:
   Recompiles and relinks C/C++ files. Only works after a complete normal
   build, but allows for much faster rebuilds of C/C++ code. For performance
   reasons, however, it skips nss, nspr, icu and ffi. This is targeted to
   improve local developer workflow when touching C/C++ code.
   Only applies to builds with ``MOZ_PSEUDO_DERECURSE``.

install-manifests
   Process install manifests. Install manifests handle the installation of
   files into the object directory.

   Unless ``NO_REMOVE=1`` is defined in the environment, files not accounted
   in the install manifests will be deleted from the object directory.

install-tests
   Processes the tests install manifest.

Common Actions
==============

The targets in this section correspond to common build-related actions. Many
of the actions in this section are effectively frontends to shell scripts.
These actions will likely all be replaced by mach commands someday.

buildsymbols
   Create a symbols archive for the current build.

   This must be performed after a successful build.

check
   Run build system tests.
