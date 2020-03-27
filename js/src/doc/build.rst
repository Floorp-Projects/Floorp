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