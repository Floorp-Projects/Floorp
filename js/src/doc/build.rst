Building SpiderMonkey
=====================

**Before you begin, make sure you have the right build tools for your
computer:**

* :ref:`Building Firefox On Linux`
* :ref:`Building Firefox On Windows`
* :ref:`Building Firefox On MacOS`
* `Others <https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions>`__

This guide shows you how to build SpiderMonkey using ``mach``, which is Mozilla's multipurpose build tool.
For builds using ``configure && make``, and translations into other languages see
:ref:`these instructions on MDN <Building SpiderMonkey>`.

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

.. code::

    # Build only the JS shell
    ac_add_options --enable-application=js

    # Disable Optimization, for the most accurate debugging experience
    ac_add_options --disable-optimize
    # Enable the debugging tools: Assertions, debug only code etc.
    ac_add_options --enable-debug

    # Use a dedicated objdir for SpiderMonkey debug builds to avoid
    # conflicting with Firefox build with default configuration.
    mk_add_options MOZ_OBJDIR=@TOPSRCDIR@/obj-debug-@CONFIG_GUESS@

To activate a particular ``MOZCONFIG``, set the environment variable:

.. code::

    export MOZCONFIG=$HOME/mozconfigs/debug

Building
--------

Once you have activated a ``MOZCONFIG`` by setting the environment variable
you can then ask ``mach``, located in the top directory of your checkout,
to do your build:

.. code::

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

.. code::

    $ ./mach jit-test

Optimized Builds
~~~~~~~~~~~~~~~~

To switch to an optimized build, one need only have an optimized build ``MOZCONFIG``,
and then activate it. An example ``$HOME/mozconfigs/optimized`` ``MOZCONFIG``
looks like this:

.. code::

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

SpiderMonkey on Android aarch64
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Building SpiderMonkey on Android
--------------------------------

- First, run `mach bootstrap` and answer `GeckoView/Firefox for Android` when
  asked which project you want to build. This will download a recent Android
  NDK, make sure all the build dependencies required to compile on Android are
  present, etc.
- Make sure that `$MOZBUILD_DIR/android-sdk-linux/platform-tools` is present in
  your `PATH` environment. You can do this by running the following line in a
  shell, or adding it to a shell profile init file:

.. code::

    $ export PATH="$PATH:~/.mozbuild/android-sdk-linux/platform-tools"

- Create a typical `mozconfig` file for compiling SpiderMonkey, as outlined in
  the :ref:`Setting up a MOZCONFIG` documentation, and include the following
  line:

.. code::

    ac_add_options --target=aarch64-linux-android

- Then compile as usual with `mach compile` with this `MOZCONFIG` file.

Running jit-tests on Android
----------------------------

- Plug your Android device to the machine which compiled the shell for aarch64
  as described above, or make sure it is on the same subnetwork as the host. It
  should appear in the list of devices seen by `adb`:

.. code::

    adb devices

This command should show you a device ID with the name of the device. If it
doesn't, make sure that you have enabled Developer options on your device, as
well as `enabled USB debugging on the device <https://developer.android.com/studio/debug/dev-options>`_.

- Run `mach jit-test --remote {JIT_TEST_ARGS}` with the android-aarch64
  `MOZCONFIG` file. This will upload the JS shell and its dependencies to the
  Android device, in a temporary directory (`/data/local/tmp/test_root/bin` as
  of 2020-09-02). Then it will start running the jit-test suite.

Debugging jit-tests on Android
------------------------------

Debugging on Android uses the GDB remote debugging protocol, so we'll set up a
GDB server on the Android device, that is going to be controlled remotely by
the host machine.

- Upload the `gdbserver` precompiled binary from the NDK from the host machine
  to the Android device, using this command on the host:

.. code::

    adb push \
        ~/.mozbuild/android-ndk-r20/prebuilt/android-arm64/gdbserver/gdbserver \
        /data/local/tmp/test_root/bin

- Make sure that the `ncurses5` library is installed on the host. On
  Debian-like distros, this can be done with `sudo apt install -y libncurses5`.

- Set up port forwarding for the GDB port, from the Android device to the host,
  so we can connect to a local port from the host, without needing to find what
  the IP address of the Android device is:

.. code::

    adb forward tcp:5039 tcp:5039

- Start `gdbserver` on the phone, passing the JS shell command line arguments
  to gdbserver:

.. code::

    adb shell export LD_LIBRARY_PATH=/data/local/tmp/test_root/bin '&&' /data/local/tmp/test_root/bin/gdbserver :5039 /data/local/tmp/test_root/bin/js /path/to/test.js

.. note::

    Note this will make the gdbserver listen on the 5039 port on all the
    network interfaces. In particular, the gdbserver will be reachable from
    every other devices on the same networks as your phone. Since the gdbserver
    protocol is unsafe, it is strongly recommended to double-check that the
    gdbserver process has properly terminated when exiting the shell, and to
    not run it more than needed.

.. note::

    You can find the full command line that the `jit_test.py` script is
    using by giving it the `-s` parameter, and copy/paste it as the final
    argument to the gdbserver invocation above.

- On the host, start the precompiled NDK version of GDB that matches your host
  architecture, passing it the path to the shell compiled with `mach` above:

.. code::

    ~/.mozbuild/android-ndk-r20/prebuilt/linux-x86_64/bin/gdb /path/to/objdir-aarch64-linux-android/dist/bin/js

- Then connect remotely to the GDB server that's listening on the Android
  device:

.. code::

    (gdb) target remote :5039
    (gdb) continue

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

.. code::

   $ rustup target add aarch64-unknown-linux-gnu

Once you have these prerequisites installed, you simply need to set a few
environment variables and configure the build appropriately:

.. code::

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

.. code::

    #!/bin/sh

    # This is the binary compiled in the previous section.
    CROSS_BIN=`dirname $0`/BUILD_AARCH64.OBJ/dist/bin/js

    # Adjust the library path as needed; this is prefixed to paths such as
    # `/lib64/libc.so.64`, and so should contain `lib` (and perhaps `lib64`)
    # subdirectories.
    exec qemu-aarch64 -L /usr/aarch64-linux-gnu/sys-root/ $CROSS_BIN "$@"

You can then invoke this wrapper as if it were a normal JS shell, and use it
with ``jit_test.py`` to run tests:

.. code::

    $ jit-test/jit_test.py ./js-cross-wrapper
