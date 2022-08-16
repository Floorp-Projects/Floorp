Application Search Engines
==========================

Firefox defines various application search engines that are shipped to users.

The extensions for the definitions of these engines live in
:searchfox:`browser/components/search/extensions <browser/components/search/extensions>`

Icons
-----

Icon Requirements
~~~~~~~~~~~~~~~~~

It is preferred that each engine is shipped with a ``.ico`` file with two sizes
of image contained within the file:

  * 16 x 16 pixels
  * 32 x 32 pixels

Some engines also have icons in
:searchfox:`browser/components/newtab/data/content/tippytop <browser/components/newtab/data/content/tippytop>`.
For these engines, there are two sizes depending on the subdirectory:

  * ``favicons/``: 32 x 32 pixels
  * ``images/``: preferred minimum of 192 x 192 pixels

Updating Icons
~~~~~~~~~~~~~~

To update icons for application search engines:

  * Place the new icon file in the :searchfox:`folder associated with the search engine <browser/components/search/extensions>`.
  * Increase the version number in the associated manifest file. This ensures
    that the add-on manager properly updates the engine.
  * Be aware that the :searchfox:`allowed-dupes.mn file <browser/installer/allowed-dupes.mn>`
    lists some icons that are intended as duplicates.

To update icons for tippytop:

  * Place the new icon file in :searchfox:`both the sub-folders within the tippytop directory <browser/components/newtab/data/content/tippytop>`.
