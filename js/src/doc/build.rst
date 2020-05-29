Building SpiderMonkey
=====================

**Before you begin, make sure you have the right build tools for your
computer:**

* :ref:`linux-build-documentation`
* `Windows <https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions/Windows_Prerequisites>`__
* `Mac <https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions/Mac_OS_X_Prerequisites>`__
* `Others <https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions>`__

This guide shows you how to build SpiderMonkey using ``mach``, which is Mozilla's multipurpose build tool.
For builds using ``configure && make``, and translations into other languages see
`these instructions on MDN <https://developer.mozilla.org/en-US/docs/Mozilla/Projects/SpiderMonkey/Build_Documentation>`__.

These instructions assume you have a clone of `mozilla-central` and are interested
in building the JS shell.

Developer (debug) build
~~~~~~~~~~~~~~~~~~~~~~~

For developing and debugging SpiderMonkey itself, it is best to have
both a debug build (for everyday debugging) and an optimized build (for
performance testing), in separate build directories. We'll start by
covering how to create a debug build.

Setting up a MOZCONFIG
-----------------------

First, we will create a ``MOZCONFIG`` file. This file describes the characteristics
of the build you'd like `mach` to create. Since it is likely you will have a
couple of ``MOZCONFIGs``, a directory like ``$HOME/mozconfigs`` is a useful thing to
have.

A basic ``MOZCONFIG`` file for doing a debug build, put into ``$HOME/mozconfigs/debug`` looks like this

.. code:: eval

    # Build only the JS shell
    ac_add_options --enable-application=js

    # Disable Optimization, for the most accurate debugging experience
    ac_add_options --disable-optimize
    # Enable the debugging tools: Assertions, debug only code etc.
    ac_add_options --enable-debug

To activate a particular ``MOZCONFIG``, set the environment variable:

.. code:: eval

    export MOZCONFIG=$HOME/mozconfigs/debug

Building
--------

Once you have activated a ``MOZCONFIG`` by setting the environment variable
you can then ask ``mach``, located in the top directory of your checkout,
to do your build:

.. code:: eval

    $ cd <path to mozilla-central>
    $ ./mach build

.. note::

   **Note**: If you are on Mac and baldrdash fails to compile with something similar to

   ::

      /usr/local/Cellar/llvm/7.0.1/lib/clang/7.0.1/include/inttypes.h:30:15: fatal error: 'inttypes.h' file not found

   This is because, starting from Mojave, headers are no longer
   installed in ``/usr/include``. Refer the `release
   notes <https://developer.apple.com/documentation/xcode_release_notes/xcode_10_release_notes>`__  under
   Command Line Tools -> New Features

   The release notes also states that this compatibility package will no longer be provided in the near
   future, so the build system on macOS will have to be adapted to look for headers in the SDK

   Until then, the following should help,

   ::

      open /Library/Developer/CommandLineTools/Packages/macOS_SDK_headers_for_macOS_10.14.pk

Once you have successfully built the shell, you can run it using ``mach run``.

Testing
--------

Once built, you can then use ``mach`` to run the ``jit-tests``:

.. code:: eval

    $ ./mach jit-test

Optimized Builds
~~~~~~~~~~~~~~~~

To switch to an optimized build, one need only have an optimized build ``MOZCONFIG``,
and then activate it. An example ``$HOME/mozconfigs/optimized`` ``MOZCONFIG``
looks like this:

.. code:: eval

    # Build only the JS shell
    ac_add_options --enable-application=js

    # Enable optimization for speed
    ac_add_options --enable-optimize
    # Enable the debugging tools: Assertions, debug only code etc.
    # For performance testing you would probably want to change this
    # to --disable-debug.
    ac_add_options --enable-debug

    # Use a separate objdir for optimized builds to allow easy
    # switching between optimized and debug builds while developing.
    mk_add_options MOZ_OBJDIR=@TOPSRCDIR@/obj-opt-@CONFIG_GUESS@

Cross-Compiling
~~~~~~~~~~~~~~~

It is possible to cross-compile a SpiderMonkey shell binary for another
architecture. For example, one can develop and compile on an x86 host while
building a ``js`` binary for AArch64 (ARM64).

Unlike the rest of this document, this section will use the old-style
``configure`` script.

To do this, first you must install the appropriate cross-compiler and system
libraries for the desired target. This is system- and distribution-specific.
Look for a package such as (using AArch64 as an example)
``aarch64-linux-gnu-gcc``. This document will assume that you have the
appropriate compiler and libraries; you can test this by compiling a C or C++
hello-world program.

You will also need the appropriate Rust compiler target support installed. For
example:

.. code:: eval

   $ rustup target add aarch64-unknown-linux-gnu

Once you have these prerequisites installed, you simply need to set a few
environment variables and configure the build appropriately:

.. code:: eval

    $ cd js/src/
    $ export CC=aarch64-linux-gnu-gcc  # adjust for target as appropriate.
    $ export CXX=aarch64-linux-gnu-g++
    $ export AR=aarch64-linux-gnu-ar
    $ export BINDGEN_CFLAGS="--sysroot /usr/aarch64-linux-gnu/sys-root"
    $ mkdir BUILD_AARCH64.OBJ
    $ cd BUILD_AARCH64.OBJ/
    $ ../configure --target=aarch64-unknown-linux-gnu
    $ make

This will produce a binary that is appropriate for the target architecture.
Note that you will not be able to run this binary natively on your host system;
to do so, keep reading to set up Qemu-based user-space emulation.

Cross-Architecture Testing using Qemu
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

It is sometimes desirable to test a cross-compiled binary directly. Unlike the
target-ISA emulators that SpiderMonkey also supports, testing a cross-compiled
binary ensures that the actual binary, running as it would on the target
system, works appropriately. As far as the JS shell is concerned, it is running
on the target ISA.

This is possible using the Qemu emulator. Qemu supports a mode called
"user-space emulation", where an individual process executes a binary that
targets a non-native ISA, and system calls are translated as appropriate to the
host system. This allows transparent execution of cross-compiled binaries.

To set this up, you will need Qemu (check your system package manager) and
shared libraries for the target system. You will likely have the necessary
shared libraries already if you cross-compiled as described above.

Then, write a small wrapper script that invokes the JS shell under Qemu. For
example:

.. code:: eval

    #!/bin/sh

    # This is the binary compiled in the previous section.
    CROSS_BIN=`dirname $0`/BUILD_AARCH64.OBJ/dist/bin/js

    # Adjust the library path as needed; this is prefixed to paths such as
    # `/lib64/libc.so.64`, and so should contain `lib` (and perhaps `lib64`)
    # subdirectories.
    exec qemu-aarch64 -L /usr/aarch64-linux-gnu/sys-root/ $CROSS_BIN "$@"

You can then invoke this wrapper as if it were a normal JS shell, and use it
with ``jit_test.py`` to run tests:

.. code:: eval

    $ jit-test/jit_test.py ./js-cross-wrapper
