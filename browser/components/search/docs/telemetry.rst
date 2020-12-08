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
  * ``sap`` (search access point) is a search that a user performs by visiting
    via one of Firefox's access points, using the associated partner codes.
  * ``sap-follow-on`` is a SAP search where the user has first accessed the site
    via a SAP, and then performed an additional search.
  * ``SERP`` refers to a search engine result page.

SEARCH_COUNTS
  This histogram records search counts for visits to SERP in-content pages. It
  also stores other items - see `address bar telemetry`_. For in-content
  searches, the format is
  ``<provider>.in-content:[sap|sap-follow-on|organic]:[code|none]``.

browser.search.with_ads
  This keyed scalar records counts of SERP pages with adverts displayed.
  The key format is ``<provider>:<sap|organic>``.

browser.search.ad_clicks
  Records clicks of adverts on SERP pages. The key format is
  ``<provider>:<sap|organic>``.

.. _address bar telemetry: /browser/urlbar/telemetry.html
.. _SearchSERPTelemetry.jsm and the associated parent/child actors: https://searchfox.org/mozilla-central/search?q=&path=SearchSERPTelemetry*.jsm&case=false&regexp=false
