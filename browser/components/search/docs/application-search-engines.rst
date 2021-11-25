Application Search Engines
==========================

Firefox defines various application search engines that are shipped to users.

The extensions for the definitions of these engines live in
`browser/components/search/extensions`_

Icons
-----

It is preferred that each engine is shipped with a ``.ico`` file with two sizes
of image contained within the file:

  * 16 x 16 pixels
  * 32 x 32 pixels

Some engines also have icons in `browser/components/newtab/data/content/tippytop`_.
For these engines, there are two sizes depending on the subdirectory:

  * ``favicons/``: 32 x 32 pixels
  * ``images/``: preferred minimum of 192 x 192 pixels

.. _browser/components/search/extensions: https://searchfox.org/mozilla-central/source/browser/components/search/extensions
.. _browser/components/newtab/data/content/tippytop: https://searchfox.org/mozilla-central/source/browser/components/newtab/data/content/tippytop
