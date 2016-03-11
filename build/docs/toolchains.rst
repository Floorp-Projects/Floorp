.. _build_toolchains:

===========================
Creating Toolchain Archives
===========================

There are various scripts in the repository for producing archives
of the build tools (e.g. compilers and linkers) required to build.

Clang
=====

See the ``build/build-clang`` directory. Read ``build/build-clang/README``
for more.

Windows
=======

The ``build/windows_toolchain.py`` script is used to build and manage
Windows toolchain archives containing Visual Studio executables, SDKs,
etc.

The way Firefox build automation works is an archive containing the
toolchain is produced and uploaded to an internal Mozilla server. The
build automation will download, verify, and extract this archive before
building. The archive is self-contained so machines don't need to install
Visual Studio, SDKs, or various other dependencies. Unfortunately,
Microsoft's terms don't allow Mozilla to distribute this archive
publicly. However, the same tool can be used to create your own copy.

Configuring Your System
-----------------------

It is **highly** recommended to perform this process on a fresh installation
of Windows 7 or 10 (such as in a VM). Installing all updates through
Windows Update is not only acceptable - it is encouraged. Although it
shouldn't matter.

Next, install Visual Studio 2015 Community. The download link can be
found at https://www.visualstudio.com/en-us/products/visual-studio-community-vs.aspx.
Be sure to follow these install instructions:

1. Choose a ``Custom`` installation and click ``Next``
2. Select ``Programming Languages`` -> ``Visual C++`` (make sure all sub items are
   selected)
3. Under ``Windows and Web Development`` uncheck everything except
   ``Universal Windows App Development Tools`` and the items under it
   (should be ``Tools (1.2)...`` and the ``Windows 10 SDK``).

Once Visual Studio 2015 Community has been installed, from a checkout
of mozilla-central, run the following to produce a ZIP archive::

   $ ./mach python build/windows_toolchain.py create-zip vs2015.zip
