Building with Debug Symbols
===========================

+--------------------------------------------------------------------+
| This page is an import from MDN and the contents might be outdated |
+--------------------------------------------------------------------+

By default, a release build of Firefox will not generate debug symbols
suitable for debugging or post-processing into the
:ref:`breakpad <Crash reporting>` symbol format. Use the
following :ref:`mozconfig <Configuring Build Options>` settings
to do a build with symbols:



Building Firefox with symbols
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

There is a single configure option to enable building with symbols on
all platforms. This is enabled by default so unless you have explicitly
disabled it your build you should include symbols.

::

   ac_add_options --enable-debug-symbols

This can optionally take an argument for the type of symbols that need
to be produced (like "-g3"). By default it uses "-g" on Linux and MacOS.
This value takes precedence over the flags set in ``MOZ_DEBUG_FLAGS``

Note that this will override the values provided for ``CFLAGS`` and
``CXXFLAGS``.


Breakpad symbol files
~~~~~~~~~~~~~~~~~~~~~

After the build is complete, run the following command to generate an
archive of :ref:`Breakpad <Crash reporting>` symbol files:

.. code:: bash

   mach buildsymbols

Treeherder uses an additional ``uploadsymbols`` target to upload
symbols to a socorro server. See
https://searchfox.org/mozilla-central/source/toolkit/crashreporter/tools/upload_symbols.py
for more information about the environment variables used by this
target.


``make package``
~~~~~~~~~~~~~~~~

If you use ``make package`` to package your build, symbols will be
stripped. If you want to keep the symbols in the patches, you need to
add this to your mozconfig:

.. code::

    ac_add_options --disable-install-strip
