.. _using_compiler_explorer:

Using Compiler Explorer
=======================

As noted in the previous section, :ref:`using_clang_query`,
it can be useful to prototype
and develop static analysis with live results. For this reason, we are running a
`custom instance of the Godbolt's Compiler Explorer <https://foxyeah.com/>`_
at foxyeah.com (behind SSO).
Our custom instance has a few tweaks, detailed below, that make it more friendly for gecko development. The biggest one is that it is bundled with our header files from mozilla-central, enabled by default.

When first started, Compiler Explorer will provide you with a default set of compiler flags that match our general compiler flags as closely as possible, as well as the mozilla headers and example source code. It will immediately compile the example code and provide the assembly output.

Quick-Start
-----------------------

Let's first ensure this matches our default toolchain for Firefox:
#. Ensure the compiler chosen says 'clang XX (mozilla-central)'
#. Find the tool button below and enable clang-query.
#. Fill-in the Recommended Boilerplate from our previous section into the stdin box containing 'Query commands'

::

  set traversal     IgnoreUnlessSpelledInSource
  set bind-root     true # Unless you use any .bind("foo") commands
  set print-matcher true
  enable output     dump


Now, you should be able to match a wide variety of C++ source code expressions.
Start with something simple like `match callExpr()` and narrow it down from here.
Continue at the next section, :ref:`writing_matchers`.

Compiler Explorer Customizations
--------------------------------

mozilla compilers
~~~~~~~~~~~~~~~~~

By default, the chosen compiler will be our current clang version, and specifically the exact compiler as downloaded from our Taskcluster cache.  That said, if we patch or upgrade the compiler this will require a manual upgrade for the CE instance, so please `let us know <https://github.com/mozilla-services/civet-docker/issues>`_ so we can do that.

mozilla headers
~~~~~~~~~~~~~~~

Under the 'Library' toolbar button for the compiler, you will see that the Mozilla and NSPR headers are available by default.  There are a lot of different ways Mozilla headers can be included and generated, so if you are seeing unexpected output the following circumstances may affect it:

#. Generated headers (which come from the obj-dir) are generated on a Linux x64 machine.
#. The default mozilla-config.h comes from the objdir as well, as therefore will specify things such as `MOZ_WEBRTC=1`
#. Non-generated headers will be extracted from moz.build files in an `imperfect way <https://github.com/mozilla-services/civet-docker/blob/00d313a7e0c55a9678bdcc39701675ac5e91bb5e/get_mozbuild_exports.py>`_ that tries to exclude Windows/OSX/Android headers. But this script may have bugs in it. If you encounter one, `feel free to file an issue <https://github.com/mozilla-services/civet-docker/issues>`_. 
#. Sometimes gecko code will get some `additional, non-global include directories <https://searchfox.org/mozilla-central/search?q=Local_includes&path=>`_ that are not available. These can be added by including a compile flag like ``-I/mozilla/central/accessible/base``.
#. We update mozilla-central and refresh the headers every hour, so they may not match your exact expected version.

compiler flags
~~~~~~~~~~~~~~

When we compile our code we use a lot of compiler flags, most of which are hidden from us.  You can see the full set of compile flags for any file by using ``./mach compilerflags path/to/file.cpp``.  By default, our Compiler Explorer instance will add most of these flags to match our compilation environment as closely as possible.  However, you can clear those flags out or edit them.

We have a custom patch on our CE instance that makes it easier to edit compiler flags - just click the Drop-Down arrow next to the small compiler flags textbox, and choose ``Detailed Compiler Flags`` to open a new editable window where you can see and edit them. In this new window, all newlines are collapsed to a single space.
