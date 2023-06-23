.. _build_visualstudio:

======================
Visual Studio Projects
======================

The build system automatically generates Visual Studio project files to aid
with development, as part of a normal ``mach build`` from the command line.

You can find the solution file at ``$OBJDIR/msvc/mozilla.sln``.

If you want to generate the project files before/without doing a full build,
running ``./mach configure && ./mach build-backend -b VisualStudio`` will do
so.


Structure of Solution
=====================

The Visual Studio solution consists of hundreds of projects spanning thousands
of files. To help with organization, the solution is divided into the following
trees/folders:

Build Targets
   This folder contains common build targets. The *full* project is used to
   perform a full build. The *binaries* project is used to build just binaries.
   The *visual-studio* project can be built to regenerate the Visual Studio
   project files.

   Performing the *clean* action on any of these targets will clean the
   *entire* build output.

Binaries
   This folder contains common binaries that can be executed from within
   Visual Studio. If you are building the Firefox desktop application,
   the *firefox* project will launch firefox.exe. You probably want one of
   these set to your startup project.

Libraries
   This folder contains entries for each static library that is produced as
   part of the build. These roughly correspond to each directory in the tree
   containing C/C++. e.g. code from ``dom/base`` will be contained in the
   ``dom_base`` project.

   These projects don't do anything when built. If you build a project here,
   the *binaries* build target project is built.

Updating Project Files
======================

Either re-running ``./mach build`` or ``./mach build-backend -b VisualStudio``
will update the Visual Studio files after the tree changes.

Moving Project Files Around
===========================

The produced Visual Studio solution and project files should be portable.
If you want to move them to a non-default directory, they should continue
to work from wherever they are. If they don't, please file a bug.

Invoking mach through Visual Studio
===================================

It's possible to run mach commands via Visual Studio. There is some light magic
involved here.

Alongside the Visual Studio project files is a batch script named ``mach.bat``.
This batch script sets the environment variables present in your *MozillaBuild*
development environment at the time of Visual Studio project generation
and invokes *mach* inside an msys shell with the arguments specified to the
batch script. This script essentially allows you to invoke mach commands
inside the MozillaBuild environment without having to load MozillaBuild.

Projects currently utilize the ``mach build`` and ``mach clobber`` commands
for building and cleaning the tree respectively. Note that running ``clobber``
deletes the Visual Studio project files, and running ``build`` recreates them.
This might cause issues while Visual Studio is running. Thus a full rebuild is
currently neither recommended, nor supported, but incremental builds should work.

The batch script does not limit its use: any mach command can be invoked.
Developers may use this fact to add custom projects and commands that invoke
other mach commands.
