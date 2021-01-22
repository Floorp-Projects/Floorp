Telemetry
=========

This section describes existing telemetry probes measuring interaction with
search engines.

Note: Some search related probes are also documented on the `address bar telemetry`_
page.

.. toctree::
   :caption: Table of Contents

   telemetry

Search probes relevant to front-end searches
--------------------------------------------

The following probes are all handled by `SearchSERPTelemetry.jsm and the associated parent/child actors`_.

Definitions:

  * ``organic`` is a search that a user performs by visiting a search engine
    directly.
  * ``SAP`` (search access point) is a search that a user performs by visiting
    via one of Firefox's access points, using the associated partner codes.
  * ``sap-follow-on`` is a SAP search where the user has first accessed the site
    via a SAP, and then performed an additional search.
  * ``tagged`` refers to a page that is tagged with an associated partner code.
    It may or may not have originated via an SAP.
  * ``SERP`` refers to a search engine result page.

SEARCH_COUNTS
  This histogram records search counts for visits to SERP in-content pages. It
  also stores other items - see `address bar telemetry`_. For in-content
  searches, the format is
  ``<provider>.in-content:[sap|sap-follow-on|organic]:[code|none]``.

browser.search.withads.*
  These keyed scalar track counts of SERP pages with adverts displayed. The key
  format is ``<provider>:<tagged|organic>``.

  They are broken down by the originating SAP where known:

  - ``urlbar``  Except search mode.
  - ``urlbar_searchmode``  Used when the Urlbar is in search mode.
  - ``searchbar``
  - ``about_home``
  - ``about_newtab``
  - ``contextmenu``
  - ``webextension``
  - ``system`` Indicates a search from the command line.
  - ``unknown`` Indicates the origin was unknown.

browser.search.adclicks.*
  This is the same as ```browser.search.withads.*`` but tracks counts for them
  clicks of adverts on SERP pages.

browser.search.with_ads
  Obsolete. This is being replaced by ``browser.search.withads.*``.

  This keyed scalar records counts of SERP pages with adverts displayed.
  The key format is ``<provider>:<sap|organic>``.

browser.search.ad_clicks
  Obsolete. This is being replaced by ``browser.search.adclicks.*``.

  Records clicks of adverts on SERP pages. The key format is
  ``<provider>:<sap|organic>``.

.. _address bar telemetry: /browser/urlbar/telemetry.html
.. _SearchSERPTelemetry.jsm and the associated parent/child actors: https://searchfox.org/mozilla-central/search?q=&path=SearchSERPTelemetry*.jsm&case=false&regexp=false
