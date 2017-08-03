.. _localization:

===================
Localization (l10n)
===================

Single-locale language repacks
==============================

To save on build time, the build system and automation collaborate to allow
downloading a packaged en-US Firefox, performing some locale-specific
post-processing, and re-packaging a locale-specific Firefox.  Such artifacts
are termed "single-locale language repacks".  There is another concept of a
"multi-locale language build", which is more like a regular build and less
like a re-packaging post-processing step.

Instructions for single-locale repacks for developers
-----------------------------------------------------

This assumes that ``$AB_CD`` is the locale you want to repack with; I tested
with "ar" and "en-GB".

#. You must have a built and packaged object directory, or a pre-built
   ``en-US`` package.

   .. code-block:: shell

      ./mach build
      ./mach package

#. Repackage using the locale-specific changes.

   .. code-block:: shell

      ./mach build installers-$AB_CD

You should find a re-packaged build at ``OBJDIR/dist/``, and a
runnable binary in ``OBJDIR/dist/l10n-stage/``.
The ``installers`` target runs quite a few things for you, including getting
the repository for the requested locale from
https://hg.mozilla.org/l10n-central/. It will clone them into
``~/.mozbuild/l10n-central``. If you have an existing repository there, you
may want to occasionally update that via ``hg pull -u``. If you prefer
to have the l10n repositories at a different location on your disk, you
can point to the directory via

   .. code-block:: shell

      ac_add_options --with-l10n-base=/make/this/a/absolute/path

Instructions for multi-locale builds
------------------------------------

If you want to create a single build with mutliple locales, you will do

#. Create a build and package

   .. code-block:: shell

      ./mach build
      ./mach package

#. For each locale you want to include in the build:

   .. code-block:: shell

      export MOZ_CHROME_MULTILOCALE="de it zh-TW"
      for AB_CD in $MOZ_CHROME_MULTILOCALE; do
         ./mach build chrome-$AB_CD
      done

#. Create the multilingual package:

   .. code-block:: shell

      AB_CD=multi ./mach package

This `currently <https://bugzilla.mozilla.org/show_bug.cgi?id=1362496>`_ only
works for Firefox for Android.
