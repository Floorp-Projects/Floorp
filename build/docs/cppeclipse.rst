.. _build_cppeclipse:

=====================
Cpp Eclipse Projects
=====================

For additional information on using Eclipse CDT see
`the MDN page
<https://developer.mozilla.org/en-US/docs/Eclipse_CDT>`_.

The build system contains alpha support for generating C++ Eclipse
project files to aid with development.

Please report bugs to bugzilla and make them depend on bug 973770.

To generate a C++ Eclipse project files, you'll need to have a fully
built tree::

   mach build

Then, simply generate the Android Eclipse build backend::

   mach build-backend -b CppEclipse

If all goes well, the path to the generated workspace should be
printed (currently, ``$OBJDIR/android_eclipse``).

To use the generated Android Eclipse project files, you'll need to
have a Eclipse CDT 8.3 (We plan to follow the latest Eclipse release)
`Eclipse CDT plugin
<https://www.eclipse.org/cdt/>`_
installed. You can then import all the projects into Eclipse using
*File > Import ... > General > Existing Projects into Workspace*
-only- if you have not ran the background indexer.

Updating Project Files
======================

As you pull and update the source tree, your C++ Eclipse files may
fall out of sync with the build configuration. The tree should still
build fine from within Eclipse, but source files may be missing and in
rare circumstances Eclipse's index may not have the proper build
configuration.

To account for this, you'll want to periodically regenerate the
Android Eclipse project files. You can do this by running ``mach build
&& mach build-backend -b CppEclipse`` from the
command line.

Currently, regeneration rewrites the original project files. **If
you've made any customizations to the projects, they will likely get
overwritten.** We would like to improve this user experience in the
future.

