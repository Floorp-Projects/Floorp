.. _mozinfo:

=======
mozinfo
=======

``mozinfo`` is a solution for representing a subset of build
configuration and run-time data.

``mozinfo`` data is typically accessed through a ``mozinfo.json`` file
which is written to the :term:`object directory` during build
configuration. The code for writing this file lives in
:py:mod:`mozbuild.mozinfo`.

``mozinfo.json`` is an object/dictionary of simple string values.

The attributes in ``mozinfo.json`` are used for many purposes. One use
is to filter tests for applicability to the current build. For more on
this, see :ref:`test_manifests`.

.. _mozinfo_attributes:

mozinfo.json Attributes
=================================

``mozinfo`` currently records the following attributes.

appname
   The application being built.

   Value comes from ``MOZ_APP_NAME`` from ``config.status``.

   Optional.

asan
   Whether address sanitization is enabled.

   Values are ``true`` and ``false``.

   Always defined.

bin_suffix
   The file suffix for binaries produced with this build.

   Values may be an empty string, as not all platforms have a binary
   suffix.

   Always defined.

bits
   The number of bits in the CPU this build targets.

   Values are typically ``32`` or ``64``.

   Universal Mac builds do not have this key defined.

   Unkown processor architectures (see ``processor`` below) may not have
   this key defined.

   Optional.

buildapp
   The path to the XUL application being built.

   For desktop Firefox, this is ``browser``. For Fennec, it's
   ``mobile/android``. For B2G, it's ``b2g``.

crashreporter
   Whether the crash reporter is enabled for this build.

   Values are ``true`` and ``false``.

   Always defined.

datareporting
  Whether data reporting (MOZ_DATA_REPORTING) is enabled for this build.

   Values are ``true`` and ``false``.

   Always defined.

debug
   Whether this is a debug build.

   Values are ``true`` and ``false``.

   Always defined.

mozconfig
   The path of the :ref:`mozconfig file <mozconfig>` used to produce this build.

   Optional.

os
   The operating system the build is produced for. Values for tier-1
   supported platforms are ``linux``, ``win``, ``mac``, ``b2g``, and
   ``android``. For other platforms, the value is the lowercase version
   of the ``OS_TARGET`` variable from ``config.status``.

   Always defined.

processor
   Information about the processor architecture this build targets.

   Values come from ``TARGET_CPU``, however some massaging may be
   performed.

   If the build is a universal build on Mac (it targets both 32-bit and
   64-bit), the value is ``universal-x86-x86_64``.

   If the value starts with ``arm``, the value is ``arm``.

   If the value starts with a string of the form ``i[3-9]86]``, the
   value is ``x86``.

   Always defined.

tests_enabled
   Whether tests are enabled for this build.

   Values are ``true`` and ``false``.

   Always defined.

toolkit
   The widget toolkit in case. The value comes from the
   ``MOZ_WIDGET_TOOLKIT`` ``config.status`` variable.

   Always defined.

topsrcdir
   The path to the source directory the build came from.

   Always defined.

