Search
======

This document describes the implementation of parts of Firefox's search interfaces.

The search area covers:

  * Search bar on the toolbar
  * In-content search
  * One-off search buttons on both the search and address bars

Search Engine handling is taken care of with the `toolkit Search Service`_.

Most of the search code lives in `browser/components/search`_.

.. toctree::

   application-search-engines
   Preferences
   telemetry

.. _toolkit Search Service: /toolkit/search/index.html
.. _browser/components/search: https://searchfox.org/mozilla-central/source/browser/components/search
