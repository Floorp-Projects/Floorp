.. _build_visualstudio:

======================
Visual Studio Projects
======================

The build system contains alpha support for generating Visual Studio
project files to aid with development.

To generate Visual Studio project files, you'll need to have a configured tree::

   mach configure

(If you have built recently, your tree is already configured.)

Then, simply generate the Visual Studio build backend::

   mach build-backend -b VisualStudio

If all goes well, the path to the generated Solution (``.sln``) file should be
printed. You should be able to open that solution with Visual Studio 2010 or
newer.

Currently, output is hard-coded to the Visual Studio 2010 format. If you open
the solution in a newer Visual Studio release, you will be prompted to upgrade
projects. Simply click through the wizard to do that.

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

As you pull and update the source tree, your Visual Studio files may fall out
of sync with the build configuration. The tree should still build fine from
within Visual Studio. But source files may be missing and IntelliSense may not
have the proper build configuration.

To account for this, you'll want to periodically regenerate the Visual Studio
project files. You can do this within Visual Studio by building the
``Build Targets :: visual-studio`` project or by running
``mach build-backend -b VisualStudio`` from the command line.

Currently, regeneration rewrites the original project files. **If you've made
any customizations to the solution or projects, they will likely get
overwritten.** We would like to improve this user experience in the
future.

Moving Project Files Around
===========================

The produced Visual Studio solution and project files should be portable.
If you want to move them to a non-default directory, they should continue
to work from wherever they are. If they don't, please file a bug.

Invoking mach through Visual Studio
===================================

It's possible to build the tree via Visual Studio. There is some light magic
involved here.

Alongside the Visual Studio project files is a batch script named ``mach.bat``.
This batch script sets the environment variables present in your *MozillaBuild*
development environment at the time of Visual Studio project generation
and invokes *mach* inside an msys shell with the arguments specified to the
batch script. This script essentially allows you to invoke mach commands
inside the MozillaBuild environment without having to load MozillaBuild.

While projects currently only utilize the ``mach build`` command, the batch
script does not limit it's use: any mach command can be invoked. Developers
may abuse this fact to add custom projects and commands that invoke other
mach commands.
