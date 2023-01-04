NSPR build instructions
=======================

Prerequisites
~~~~~~~~~~~~~

On Windows, the NSPR build system needs GNU make and a Unix command-line
utility suite such as MKS Toolkit, Cygwin, and MSYS. The easiest way to
get these tools is to install the
:ref:`MozillaBuild` package.

Introduction
~~~~~~~~~~~~

The top level of the NSPR source tree is the ``mozilla/nsprpub``
directory. Although ``nsprpub`` is a subdirectory under ``mozilla``,
NSPR is independent of the Mozilla client source tree.

Building NSPR consists of three steps:

#. run the configure script. You may override the compilers (the CC
   environment variable) or specify options.
#. build the libraries
#. build the test programs

For example,

::

   # check out the source tree from Mercurial
   hg clone https://hg.mozilla.org/projects/nspr
   # create a build directory
   mkdir target.debug
   cd target.debug
   # run the configure script
   ../nspr/configure [optional configure options]
   # build the libraries
   gmake
   # build the test programs
   cd pr/tests
   gmake

On Mac OS X, use ``make``, which is GNU ``make``.

.. _Configure_options:

Configure options
~~~~~~~~~~~~~~~~~

Although NSPR uses autoconf, its configure script has two default values
that are different from most open source projects.

#. If the OS vendor provides a compiler (for example, Sun and HP), NSPR
   uses that compiler instead of GCC by default.
#. NSPR build generates a debug build by default.

.. _--disable-debug_--enable-optimize:

--disable-debug --enable-optimize
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Specify these two options to generate an optimized (release) build.

These two options can actually be used independently, but it's not
recommended.

--enable-64bit
^^^^^^^^^^^^^^

On a dual 32-bit/64-bit platform, NSPR build generates a 32-bit build by
default. To generate a 64-bit build, specify the ``--enable-64bit``
configure option.

.. _--targetx86_64-pc-mingw32:

--target=x86_64-pc-mingw32
^^^^^^^^^^^^^^^^^^^^^^^^^^

For 64-bit builds on Windows, when using the mozbuild environment.

.. _--enable-win32-target.3DWIN95:

--enable-win32-target=WIN95
^^^^^^^^^^^^^^^^^^^^^^^^^^^

This option is only used on Windows. NSPR build generates a "WINNT"
configuration by default on Windows for historical reasons. We recommend
most applications use the "WIN95" configuration. The "WIN95"
configuration supports all versions of Windows. The "WIN95" name is
historical; it should have been named "WIN32".

To generate a "WIN95" configuration, specify the
``--enable-win32-target=WIN95`` configure option.

.. _--enable-debug-rtl:

--enable-debug-rtl
^^^^^^^^^^^^^^^^^^

This option is only used on Windows. NSPR debug build uses the release C
run-time library by default. To generate a debug build that uses the
debug C run-time library, specify the ``--enable-debug-rtl`` configure
option.

.. _Makefile_targets:

Makefile targets
~~~~~~~~~~~~~~~~

-  all (default)
-  clean
-  realclean
-  distclean
-  install
-  release

.. _Running_the_test_programs:

Running the test programs
~~~~~~~~~~~~~~~~~~~~~~~~~

The tests were built above, in the ``pr/tests`` directory.

On Mac OS X, they can be executed with the following:

.. code::

    /bin/sh:

    $ cd pr/tests
    $ DYLD_LIBRARY_PATH=../../dist/lib ./accept
    PASS
    $
    $ # to run all the NSPR tests...
    $
    $ DYLD_LIBRARY_PATH=../../dist/lib ../../../nspr/pr/tests/runtests.sh ../..
