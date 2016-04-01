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

There are scripts in-tree in mozharness to orchestrate these re-packaging
steps for `Desktop
<https://dxr.mozilla.org/mozilla-central/source/testing/mozharness/scripts/desktop_l10n.py>`_
and `Android
<https://dxr.mozilla.org/mozilla-central/source/testing/mozharness/scripts/mobile_l10n.py>`_
but they rely heavily on buildbot information so they are almost impossible to
run locally.

The following instructions are extracted from the `Android script with hg hash
494289c7
<https://dxr.mozilla.org/mozilla-central/rev/494289c72ba3997183e7b5beaca3e0447ecaf96d/testing/mozharness/scripts/mobile_l10n.py>`_,
and may need to be updated and slightly modified for Desktop.

Step by step instructions for Android
-------------------------------------

This assumes that ``$AB_CD`` is the locale you want to repack with; I tested
with "ar" and "en-GB".

.. warning:: l10n repacks do not work with artifact builds.  Repackaging
   compiles no code so supporting ``--disable-compile-environment`` would not
   save much, if any, time.

#. You must have a built and packaged object directory, or a pre-built
   ``en-US`` package.

   .. code-block:: shell

      ./mach build
      ./mach package

#. Clone ``l10n-central/$AB_CD`` so that it is a sibling to your
   ``mozilla-central`` directory.

   .. code-block:: shell

      $ ls -al
      mozilla-central
      ...
      $ mkdir -p l10n-central
      $ hg clone https://hg.mozilla.org/l10n-central/$AB_CD l10n-central/$AB_CD
      $ ls -al
      mozilla-central
      l10n-central/$AB_CD
      ...

#. Copy your ``mozconfig`` to ``mozconfig.l10n`` and add the following.

   ::

      ac_add_options --with-l10n-base=../../l10n-central
      ac_add_options --disable-tests
      mk_add_options MOZ_OBJDIR=./objdir-l10n

#. Configure and prepare the l10n object directory.

   .. code-block:: shell

      MOZCONFIG=mozconfig.l10n ./mach configure
      MOZCONFIG=mozconfig.l10n ./mach build -C config export
      MOZCONFIG=mozconfig.l10n ./mach build buildid.h

#. Copy your built package and unpack it into the l10n object directory.

   .. code-block:: shell

      cp $OBJDIR/dist/fennec-*en-US*.apk ./objdir-l10n/dist
      MOZCONFIG=mozconfig.l10n ./mach build -C mobile/android/locales unpack

#. Run the ``compare-locales`` script to write locale-specific changes into
   ``objdir-l10n/merged``.

   .. code-block:: shell

      MOZCONFIG=mozconfig.l10n ./mach compare-locales --merge-dir objdir-l10n/merged $AB_CD

#. Finally, repackage using the locale-specific changes.

   .. code-block:: shell

      MOZCONFIG=mozconfig.l10n LOCALE_MERGEDIR=`realpath objdir-l10n/merged` ./mach build -C mobile/android/locales installers-$AB_CD

   (Note the absolute path for ``LOCALE_MERGEDIR``.)  You should find a
   re-packaged build at ``objdir-l10n/dist/fennec-*$AB_CD*.apk``.
