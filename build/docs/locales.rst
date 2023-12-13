.. _localization:

================
Localized Builds
================

Localization repacks
====================

To save on build time, the build system and automation collaborate to allow
downloading a packaged en-US Firefox, performing some locale-specific
post-processing, and re-packaging a locale-specific Firefox. Such artifacts
are termed "single-locale language repacks". There is another concept of a
"multi-locale language build", which is more like a regular build and less
like a re-packaging post-processing step.

.. note::

  These builds rely on make targets that don't work for
  `artifact builds <https://bugzilla.mozilla.org/show_bug.cgi?id=1387485>`_.

Instructions for single-locale repacks for developers
-----------------------------------------------------

This assumes that ``$AB_CD`` is the locale you want to repack with; you
find the available localizations on `l10n-central <https://hg.mozilla.org/l10n-central/>`_.

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

This build also packages a language pack.

Instructions for language packs
-------------------------------

Language packs are extensions that contain just the localized resources. Building
them doesn't require an actual build, but they're only compatible with the
``mozilla-central`` source they're built with.


.. code-block:: shell

  ./mach build langpack-$AB_CD

This target shares much of the logic of the ``installers-$AB_CD`` target above,
and does the check-out of the localization repository etc. It doesn't require
a package or a build, though. The generated language pack is in
``OBJDIR/dist/$(MOZ_PKG_PLATFORM)/xpi/``.

.. note::

  Despite the platform-dependent location in the build directory, language packs
  are platform independent, and the content that goes into them needs to be
  built in a platform-independent way.

Instructions for multi-locale builds
------------------------------------

If you want to create a single build with multiple locales, you will do

#. Create a build and package

   .. code-block:: shell

      ./mach build
      ./mach package

#. Create the multi-locale package:

   .. code-block:: shell

      ./mach package-multi-locale --locales de it zh-TW

On Android, this produces a multi-locale GeckoView AAR and multi-locale APKs,
including GeckoViewExample.  You can test different locales by changing your
Android OS locale and restarting GeckoViewExample.  You'll need to install with
the ``MOZ_CHROME_MULTILOCALE`` variable set, like:

   .. code-block:: shell

       env MOZ_CHROME_MULTILOCALE=en-US,de,it,zh-TW ./mach android install-geckoview_example

Multi-locale builds without compiling
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

For deep technical reasons, artifact builds do not support multi-locale builds.
However, with a little work, we can achieve the same effect:

#. Arrange a ``mozconfig`` without a compilation environment but with support
   for the ``RecursiveMake`` build backend, like:

   .. code-block:: shell

      ac_add_options --disable-compile-environment
      export BUILD_BACKENDS=FasterMake,RecursiveMake
      ... other options ...

#. Configure.

   .. code-block:: shell

      ./mach configure

#. Manually provide compiled artifacts.

   .. code-block:: shell

      ./mach artifact install [-v]

#. Build.

   .. code-block:: shell

      ./mach build

#. Produce a multi-locale package.

   .. code-block:: shell

      ./mach package-multi-locale --locales de it zh-TW

This build configuration is fragile and not generally useful for active
development (for that, use a full/compiled build), but it certainly speeds
testing multi-locale packaging.

General flow of repacks
-----------------------

The general flow of the locale repacks is controlled by
``$MOZ_BUILD_APP/locales/Makefile.in`` and ``toolkit/locales/l10n.mk``, plus
the packaging build system. The three main entry points above all trigger
related build flows:

#. Get the localization repository, if needed
#. Run l10n-merge with a prior clobber of the merge dir
#. Copy l10n files to ``dist``, with minor differences here between ``l10n-%`` and ``chrome-%``
#. Repackage and package

Details on l10n-merge are described in its own section below.
The copying of files is mainly controlled by ``jar.mn``, in the few source
directories that include localizable files. ``l10n-%`` is used for repacks,
``chrome-%`` for multi-locale packages. The repackaging is dedicated
Python code in ``toolkit/mozapps/installer/l10n-repack.py``, using an existing
package. It strips existing ``chrome`` l10n resources, and adds localizations
and metadata.

Language packs don't require repackaging. The windows installers are generated
by merely packaging an existing repackaged zip into to an installer.

Exposing strings
================

The localization flow handles a few file formats in well-known locations in the
source tree.

Alongside being built by including the directory in ``$MOZ_BUILD_APP/locales/Makefile.in``
and respective entries in a ``jar.mn``, we also have configuration files tailored
to localization tools and infrastructure. They're also controlling which
files l10n-merge handles, and how.

These configurations are TOML files. They're part of the bigger
localization ecosystem at Mozilla, and `the documentation about the
file format <http://moz-l10n-config.readthedocs.io/en/latest/fileformat.html>`_
explains how to set them up, and what the entries mean. In short, you find

.. code-block:: toml

   [paths]
   reference = browser/locales/en-US/**
   l10n = {l}browser/**

to add a directory for all localizations. Changes to these files are best
submitted for review by :Pike or :flod.

These configuration files are the future, and right now, we still have
support for the previous way to configuring l10n, which is described below.

The locations are commonly in directories like

    :file:`browser/`\ ``locales/en-US/``\ :file:`subdir/file.ext`

The first thing to note is that only files beneath :file:`locales/en-US` are
exposed to localizers. The second thing to note is that only a few directories
are exposed. Which directories are exposed is defined in files called
``l10n.ini``, which are at a
`few places <https://searchfox.org/mozilla-central/search?q=path%3Al10n.ini&redirect=true>`_
in the source code.

An example looks like this

.. code-block:: ini

    [general]
    depth = ../..

    [compare]
    dirs = browser
        browser/branding/official

    [includes]
    toolkit = toolkit/locales/l10n.ini

This tells the l10n infrastructure three things:

* resolve the paths against the directory two levels up
* include files in :file:`browser/locales/en-US` and
  :file:`browser/branding/official/locales/en-US`
* load more data from :file:`toolkit/locales/l10n.ini`

For projects like Thunderbird and SeaMonkey in ``comm-central``, additional
data needs to be provided when including an ``l10n.ini`` from a different
repository:

.. code-block:: ini

    [include_toolkit]
    type = hg
    mozilla = mozilla-central
    repo = https://hg.mozilla.org/
    l10n.ini = toolkit/locales/l10n.ini

This tells the l10n infrastructure where to find the repository, and where inside
that repository the ``l10n.ini`` file is. This is needed because for local
builds, :file:`mail/locales/l10n.ini` references
:file:`mozilla/toolkit/locales/l10n.ini`, which is where the comm-central
build setup expects toolkit to be.

Now that the directories exposed to l10n are known, we can talk about the
supported file formats.

File formats
------------

The following file formats are known to the l10n tool chains:

Fluent
    Used in Firefox UI, both declarative and programmatically.
Properties
    Used from JavaScript and C++. When used from js, also comes with
    plural support (avoid if possible).
ini
    Used by the crashreporter and updater, avoid if possible.

Adding new formats involves changing various different tools, and is strongly
discouraged.

Exceptions
----------
Generally, anything that exists in ``en-US`` needs a one-to-one mapping in
all localizations. There are a few cases where that's not wanted, notably
around locale configuration and locale-dependent metadata.

For optional strings and files, l10n-merge won't add ``en-US`` content if
the localization doesn't have that content.

For the TOML files, the
`[[filters]] documentation <https://moz-l10n-config.readthedocs.io/en/latest/fileformat.html#filters>`_
is a good reference. In short, filters match the localized source code, optionally
a ``key``, and an action. An example like

.. code-block:: toml

  [filters]
  path = "{l}calendar/chrome/calendar/calendar-event-dialog.properties"
  key = "re:.*Nounclass[1-9].*"
  action = "ignore"

indicates that the matching messages in ``calendar-event-dialog.properties`` are optional.

For the legacy ini configuration files, there's a Python module
``filter.py`` next to the main ``l10n.ini``, implementing :py:func:`test`, with the following
signature

.. code-block:: python

    def test(mod, path, entity = None):
        if does_not_matter:
            return "ignore"
        if show_but_do_not_merge:
            return "report"
        # default behavior, localizer or build need to do something
        return "error"

For any missing file, this function is called with ``mod`` being
the *module*, and ``path`` being the relative path inside
:file:`locales/en-US`. The module is the top-level dir as referenced in
:file:`l10n.ini`.

For missing strings, the :py:data:`entity` parameter is the key of the string
in the en-US file.

l10n-merge
==========

The chrome registry in Gecko doesn't support fallback from a localization to ``en-US`` at runtime.
Thus, the build needs to ensure that the localization as it's built into
the package has all required strings, and that the strings don't contain
errors. To ensure that, we're *merging* the localization and ``en-US``
at build time, nick-named l10n-merge.

For Fluent, we're also removing erroneous messages. For many errors in Fluent,
that's cosmetic, but when a localization has different values or attributes
on a message, that's actually important so that the DOM bindings of Fluent
can apply the translation without having to load the ``en-US`` source to
compare against.

The process can be manually triggered via

.. code-block:: bash

    $> ./mach build merge-$AB_CD

It creates another directory in the object dir, :file:`browser/locales/merge-dir/$AB_CD`, in
which the sanitized files are stored. The actual repackaging process only looks
in the merged directory, so the preparation steps of l10n-merge need to ensure
that all files are generated or copied.

l10n-merge modifies a file if it supports the particular file type, and there
are missing strings which are not filtered out, or if an existing string
shows an error. See the Checks section below for details. If the files are
not modified, l10n-merge copies them over to the respective location in the
merge dir.

Checks
------

As part of the build and other localization tool chains, we run a variety
of source-based checks. Think of them as linters.

The suite of checks is usually determined by file type, i.e., there's a
suite of checks for Fluent files and one for properties files, etc.

Localizations
-------------

Now that we talked in-depth about how to expose content to localizers,
where are the localizations?

We host a mercurial repository per locale. All of our
localizations can be found on https://hg.mozilla.org/l10n-central/.

You can search inside our localized files on
`Transvision <https://transvision.mozfr.org/>`_.
