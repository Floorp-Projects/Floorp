.. _build_androideclipse:

========================
Android Eclipse Projects
========================

The build system contains alpha support for generating Android Eclipse
project files to aid with development.

To generate Android Eclipse project files, you'll need to have a fully
built and packaged tree::

   mach build && mach package

(This is because Eclipse itself packages an APK containing
``omni.ja``, and ``omni.ja`` is only assembled during packaging.)

Then, simply generate the Android Eclipse build backend::

   mach build-backend -b AndroidEclipse

If all goes well, the path to the generated projects should be
printed (currently, ``$OBJDIR/android_eclipse``).

To use the generated Android Eclipse project files, you'll need to
have a recent version of Eclipse (see `Tested Versions`_) with the
`Eclipse ADT plugin
<http://developer.android.com/tools/sdk/eclipse-adt.html>`_
installed. You can then import all the projects into Eclipse using
*File > Import ... > General > Existing Projects into Workspace*.

Updating Project Files
======================

As you pull and update the source tree, your Android Eclipse files may
fall out of sync with the build configuration. The tree should still
build fine from within Eclipse, but source files may be missing and in
rare circumstances Eclipse's index may not have the proper build
configuration.

To account for this, you'll want to periodically regenerate the
Android Eclipse project files. You can do this by running ``mach build
&& mach package && mach build-backend -b AndroidEclipse`` from the
command line. It's a good idea to refresh and clean build all projects
in Eclipse after doing this.

In future, we'd like to include an Android Eclipse run configuration
or build target that integrates updating the project files.

Currently, regeneration rewrites the original project files. **If
you've made any customizations to the projects, they will likely get
overwritten.** We would like to improve this user experience in the
future.

Troubleshooting
===============

If Eclipse's builder gets confused, you should always refresh and
clean build all projects. If Eclipse's builder is continually
confused, you can see a log of what is happening at
``$OBJDIR/android_eclipse/build.log``.

If you run into memory problems executing ``dex``, you should
`Increase Eclipse's memory limits <http://stackoverflow.com/a/11093228>`_.

The produced Android Eclipse project files are unfortunately not
portable. Please don't move them around.

Structure of Android Eclipse projects
=====================================

The Android Eclipse backend generates several projects spanning Fennec
itself and its tests. You'll mostly interact with the *Fennec* project
itself.

In future, we'd like to expand this documentation to include some of
the technical details of how the Eclipse integration works, and how to
add additional Android Eclipse projects using the ``moz.build``
system.

Tested Versions
===============

===============    ====================================    =================
OS                 Version                                 Working as of
===============    ====================================    =================
Mac OS X           Luna (Build id: 20130919-0819)          February 2014
Mac OS X           Kepler (Build id: 20131219-0014)        February 2014
Mac OS X 10.8.5    Kepler (Build id: 20130919-0819)        February 2014
===============    ====================================    =================
