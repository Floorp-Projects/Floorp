Adding new words to the en-US dictionary
========================================

Occasionally bugs are filed pointing out situations where perfectly
legitimate words are missing from the English spell check dictionary in
Firefox. This article describes the process for adding a word to the
dictionary.

The process is pretty straight-forward:

#. Get a clone of mozilla-central (see :ref:`Firefox Contributors' Quick Reference`), if
   you don't already have one, and make sure you can build it
   successfully.
#. Get into the dictionary sources directory using this command:
   ``cd extensions/spellcheck/locales/en-US/hunspell/dictionary-sources``
#. There's a special script used for editing dictionaries. The script
   only works if you have the environment variable ``EDITOR`` set to the
   executable of an editor program; if you don't have it set, you can use
   ``EDITOR=vim sh edit-dictionary`` to edit using vim (or you can
   substitute some other editor), or you can just type
   ``sh edit-dictionary`` if you have an ``EDITOR`` already specified.
#. Add and remove words in the dictionary file, then quit the editor.
#. Use ``sh merge-dictionaries`` to process the dictionary changes you've
   made.
#. Move the revised dictionary file into position: ``mv en-US.dic ..``
#. Build Firefox and test your updated dictionary. Once you're
   satisfied, use the process described in :ref:`write_a_patch` to create a
   patch.
