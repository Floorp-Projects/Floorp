======================================
Managing the built-in en-US dictionary
======================================

The en-US build of Firefox includes a built-in Hunspell dictionary based on the
`SCOWL`_ dataset. This document describes the process to add new words to the
dictionary, or update it to the current upstream version.

For more information about Hunspell or the affix file format, you can check
`the Ubuntu man page for hunspell
<https://manpages.ubuntu.com/manpages/bionic/man5/hunspell.5.html>`_.

Requesting to add new words to the en-US dictionary
===================================================

If you’d like to add new words to the dictionary, you can add your request to
`this bug <https://bugzilla.mozilla.org/show_bug.cgi?id=enus-dictionary>`_:

* Include all possible forms, e.g. plural and genitive forms for nouns,
  different tenses for verbs.
* Try to provide information on the terms you want to add, in particular
  references to external sources that confirm the usage of the term (e.g.
  Merriam-Webster or Oxford online dictionaries).

.. note::

  If you’re fixing the existing bug with pending requests, make sure to `file a
  new bug`_ and move the alias ``enus-dictionary`` (in the *Details* section)
  from the old bug to the new one.

Adding new words to the en-US dictionary
========================================

This section describes the process for adding new words to the dictionary:

#. Get a clone of mozilla-central (see :ref:`Firefox Contributors' Quick
   Reference`), if you don’t already have one, and make sure you can build it
   successfully.
#. Move in the dictionary sources directory using this command:
   ``cd extensions/spellcheck/locales/en-US/hunspell/dictionary-sources``.
#. Identify the current version of SCOWL by checking the file
   ``README_en_US.txt`` (at the beginning of the file there is a line similar to
   ``Generated from SCOWL Version 2020.12.07``, where ``2020.12.07`` is the
   SCOWL version).
#. Download the same version of the dictionary from the `SCOWL`_ homepage or
   `SourceForce`_ as a tarball (tag.gz) and unpack it in the working directory.
   Rename the resulting folder from ``scowl-YYYY.MM.DD`` to ``scowl``.
#. There’s a special script used for editing dictionaries. The script
   only works if you have the environment variable ``EDITOR`` set to the
   executable of an editor program; if you don’t have it set, you can use
   ``EDITOR=vim sh edit-dictionary.sh`` to edit using ``vim`` (or you can
   substitute it with another editor), or you can just type
   ``sh edit-dictionary.sh`` if you have an ``EDITOR`` already specified.

   Copy and paste the full list of words, then save and quit the editor. It’s
   not necessary to put the words in alphabetical order, as it will be corrected
   by the script.
#. Run the script ``sh make-new-dict.sh`` to generate a new dictionary and make
   sure it runs without errors. For more details on this script, see the
   `make-new-dict.sh`_ section.
#. Do a sanity check on the resulting dictionary file ``en_US-mozilla.dic``. For
   example, make sure that the size is about the same as the original dictionary
   (or slightly larger).
#. If everything looks correct, use ``sh install-new-dict.sh`` to copy the
   generated file in the right position.
#. Build Firefox and test your updated dictionary. Once you’re
   satisfied, use the process described in :ref:`write_a_patch` to create a
   patch.

Note that the update script will modify 2 versions of the dictionary, and both
need to be committed:

* ``en-US.dic``: the dictionary actually shipping in the build, it uses
  ISO-8859-1 encoding.
* ``utf8/en-US.dic``: a version of the same dictionary with UTF-8 encoding. This
  is used to work around issues with Phabricator, and it allows to display
  actual changes in the diff.

Exclude words from suggestions
==============================

It’s possible to completely exclude words from suggested alternatives by adding
an affix rule ``!`` at the end of the definition in the ``.dic`` file. For
example:

* ``bum`` would be changed to ``bum/!`` (note the additional forward slash).
* ``bum/MS`` would be changed to ``bum/MS!``.

In order to exclude a word from suggestions, follow the instructions available
in `Adding new words to the en-US dictionary`_. Instead of running the
``edit-dictionary.sh`` script (point 5), use a text editor to edit the file
``en-US.dic`` directly, then proceed with the remaining instructions.

.. warning::

  Make sure to open ``en-US.dic`` with the correct encoding. For example, Visual
  Studio Code will try to open it as ``UTF-8``, and it needs to be reopened with
  encoding ``Western (ISO 8859-1)``.

Upgrading dictionary to a new upstream version of SCOWL
=======================================================

The English dictionary available in mozilla-central is based on the
`SCOWL`_ dictionary. Some scripts distributed with the SCOWL package are
used to generate the files for the en-US dictionary.

The working directory for this process is
``extensions/spellcheck/locales/en-US/hunspell/dictionary-sources``.

#. Download the latest version of the dictionary from the `SCOWL`_ homepage or
   `SourceForce`_ as a tarball (tag.gz) and unpack it in the working directory.
   Rename the resulting folder from ``scowl-YYYY.MM.DD`` to ``scowl``.
#. Run the script ``sh make-new-dict.sh`` to generate a new dictionary and make
   sure it runs without errors. For more details on this script, see the
   `make-new-dict.sh`_ section.
#. Do a sanity check on the resulting dictionary file ``en_US-mozilla.dic``. For
   example, make sure that the size is about the same as the original dictionary
   (or slightly larger).
#. If everything looks correct, use ``sh install-new-dict.sh`` to copy the
   generated file in the right position and use the process described in
   :ref:`write_a_patch` to create a patch.

Info about the file structure
=============================

mozilla-specific.txt
--------------------

This file contains Mozilla-specific words that should not be submitted
upstream. For example, ``Firefox`` should go in this file (see `bug 237921`_).

Note that the file ``5-mozilla-specific.txt`` is generated by expanding
``mozilla-specific.txt`` and should not be edited directly.

utf8 folder
-----------

``dictionary-sources/utf8`` is used to store a copy with UTF-8 encoding of the
dictionary files. This is used to work around limitations in Phabricator, which
treats ISO-8859-1 files as binary and won’t display a diff when updating them.

Info about the included scripts
===============================

make-new-dict.sh
----------------

The dictionary upgrade scripts ``make-new-dict.sh`` works by expanding (i.e.
“unmunching”) the affix compression dictionaries to create wordlists and
use those to generate a new dictionary.

The upgrade script expects the current upstream version to be kept in the
directory ``orig``.

The script will create a few files in ``dictionary-sources/support_file`` in the
following order:

* ``0-special.txt`` contains numbers and ordinals expanded from SCOWL
  ``en.dic.supp``.
* ``1-base.txt`` contains words expanded from ``en_US-custom.dic`` in the
  **previous** version of SCOWL (from the ``orig`` folder).
* ``2-mozilla.txt`` contains words expanded from the current Mozilla dictionary.
* ``3-upstream.txt`` contains words expanded from ``en_US-custom.dic`` in the
  **new** version of SCOWL (from the ``scowl/speller`` folder).
* ``2-mozilla-removed.txt`` contains words that are only available in the SCOWL
  dictionary, i.e. removed by Mozilla.
* ``2-mozilla-added.txt`` contains words that are only available in the current
  Mozilla dictionary, i.e. added by Mozilla.
* ``4-patched.txt`` contains words from the new SCOWL dictionary
  (``3-upstream.txt``), with words from (``2-mozilla-removed.txt``) removed and
  words (``2-mozilla-added.txt``) added.
* ``5-mozilla-specific.txt`` is expanded from ``mozilla-specific.txt`` using the
  current affix rules from the Mozilla dictionary.
* ``5-mozilla-removed.txt`` and ``5-mozilla-added.txt`` contain words that are
  respectively removed and added by Mozilla compared to the **new** SCOWL
  version. These files could be used to submit upstream changes, but words
  included in ``5-mozilla-specific.txt`` should be removed from this list.

The new dictionary is available as ``en_US-mozilla.dic`` and should be copied
over using the ``install-new-dict.sh`` script.

install-new-dict.sh
-------------------

The script:

* Creates a copy of ``orig`` as ``support_files/orig-bk`` and copies the new
  upstream version to ``orig``.
* Copies the existing Mozilla dictionary in ``support_files/mozilla-bk``.
* Converts the dictionary (.dic) generated by ``make-new-dict.sh`` from UTF-8 to
  ISO-8859-1 and moves it to the parent folder.
* Sets the affix file (.aff) to use ``ISO8859-1`` as ``SET`` instead of the
  original ``UTF-8``, removes ``ICONV`` patterns (input conversion tables).


.. _SCOWL: http://wordlist.aspell.net
.. _file a new bug: https://bugzilla.mozilla.org/show_bug.cgi?id=enus-dictionary
.. _SourceForce: https://sourceforge.net/projects/wordlist/files/SCOWL/
.. _bug 237921: https://bugzilla.mozilla.org/show_bug.cgi?id=237921
