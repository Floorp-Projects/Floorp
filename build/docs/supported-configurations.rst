.. _build_supported_configurations:

========================
Supported Configurations
========================

This page attempts to document supported build configurations.

Windows
=======

We support building on Windows XP and newer operating systems using
Visual Studio 2010 and newer.

The following are not fully supported by Mozilla (but may work):

* Building without the latest *MozillaBuild* Windows development
  environment
* Building with Mingw or any other non-Visual Studio toolchain.

OS X
====

We support building on OS X 10.6 and newer with the OS X 10.6 SDK.

The tree should build with the following OS X releases and SDK versions:

* 10.6 Snow Leopard
* 10.7 Lion
* 10.8 Mountain Lion
* 10.9 Mavericks

The tree requires building with Clang 3.3 and newer. This corresponds to
version of 4.2 of Apple's Clang that ships with Xcode. This corresponds
to Xcode 4.6 and newer. Xcode 4.6 only runs on OS X 10.7.4 and newer.
So, OS X 10.6 users will need to install a non-Apple toolchain. Running
``mach bootstrap`` should install an appropriate toolchain from Homebrew
or MacPorts automatically.

The tree should build with GCC 4.4 and newer on OS X. However, this
build configuration isn't as widely used (and differs from what Mozilla
uses to produce OS X builds), so it's recommended to stick with Clang.

Linux
=====

Linux 2.6 and later kernels are supported.

Most distributions are supported as long as the proper package
dependencies are in place. Running ``mach bootstrap`` should install
packages for popular Linux distributions. ``configure`` will typically
detect missing dependencies and inform you how to disable features to
work around unsatisfied dependencies.

Clang 3.3 or GCC 4.4 is required to build the tree.
