Telemetry
=========

This section describes existing telemetry probes measuring interaction with
search engines from the browser UI.

Other search-related telemetry is recorded by Toolkit such as search service
telemetry and telemetry related to fetching search suggestions. Toolkit search
telemetry is relevant to Firefox as well as other consumers of Toolkit. See
:doc:`/toolkit/search/Telemetry` in the Toolkit documentation for details.

.. toctree::
   :caption: Table of Contents

   telemetry

Definitions
-----------

  * ``organic`` is a search that a user performs by visiting a search engine
    directly.
  * ``SAP`` (search access point) is a search that a user performs by visiting
    via one of Firefox's access points, using the associated partner codes.
  * ``sap-follow-on`` is a SAP search where the user has first accessed the site
    via a SAP, and then performed an additional search.
  * ``tagged`` refers to a page that is tagged with an associated partner code.
    It may or may not have originated via an SAP.
  * ``SERP`` refers to a search engine result page.

Search probes relevant to front-end searches
--------------------------------------------

The Address Bar is an integral part of search and has `additional telemetry of its own`_.

BrowserSearchTelemetry.jsm
~~~~~~~~~~~~~~~~~~~~~~~~~~

This telemetry is handled by `BrowserSearchTelemetry.jsm`_.

SEARCH_COUNTS - SAP usage
  This histogram tracks search engines and Search Access Points. It is augmented
  by multiple SAPs, including the urlbar.
  It's a keyed histogram, the keys are strings made up of search engine names
  and SAP names, for example ``google.urlbar``.
  For each key, this records the count of searches made using that engine and SAP.
  SAP names can be:

    - ``alias`` This is when using an alias (like ``@google``) in the urlbar.
      Note there is often confusion between the terms alias and keyword, and
      they may be used inappropriately: aliases refer to search engines, while
      keywords refer to bookmarks. We expect no results for this SAP in Firefox
      83+, since urlbar-searchmode replaces it.
    - ``abouthome``
    - ``contextmenu``
    - ``newtab``
    - ``searchbar``
    - ``system``
    - ``urlbar`` Except aliases and search mode.
    - ``urlbar-handoff`` Used when searching from about:newtab.
    - ``urlbar-searchmode`` Used when the Urlbar is in search mode.
    - ``webextension``

browser.engagement.navigation.*
  These keyed scalars track search through different SAPs, for example the
  urlbar is tracked by ``browser.engagement.navigation.urlbar``.
  It counts loads triggered in a subsession from the specified SAP, broken down
  by the originating action.
  Possible SAPs are:

    - ``urlbar``  Except search mode.
    - ``urlbar_handoff`` Used when searching from about:newtab.
    - ``urlbar_searchmode``  Used when the Urlbar is in search mode.
    - ``searchbar``
    - ``about_home``
    - ``about_newtab``
    - ``contextmenu``
    - ``webextension``
    - ``system`` Indicates a search from the command line.

  Recorded actions may be:

    - ``search``
      Used for any search from ``contextmenu``, ``system`` and ``webextension``.
    - ``search_alias``
      For ``urlbar``, indicates the user confirmed a search through an alias.
    - ``search_enter``
      For ``about_home`` and ``about:newtab`` this counts any search.
      For the other SAPs it tracks typing and then pressing Enter.
    - ``search_formhistory``
      For ``urlbar``, indicates the user picked a form history result.
    - ``search_oneoff``
      For ``urlbar`` or ``searchbar``, indicates the user confirmed a search
      using a one-off button.
    - ``search_suggestion``
      For ``urlbar`` or ``searchbar``, indicates the user confirmed a search
      suggestion.

navigation.search (OBSOLETE)
  This is a legacy and disabled event telemetry that is currently under
  discussion for removal or modernization. It can't be enabled through a pref.
  it's more or less equivalent to browser.engagement.navigation, but can also
  report the picked search engine.

SearchSERPTelemetry.jsm
~~~~~~~~~~~~~~~~~~~~~~~

This telemetry is handled by `SearchSERPTelemetry.jsm and the associated parent/child actors`_.

SEARCH_COUNTS - SERP results
  This histogram records search counts for visits to SERP in-content pages.
  For in-content searches, the format is
  ``<provider>.in-content:[sap|sap-follow-on|organic]:[<code>|other|none]``.

  This is obsolete, browser.search.content.* should be preferred.

browser.search.content.*
  These keyed scalar track counts of SERP page loads. The key format is
  ``<provider>:[tagged|tagged-follow-on|organic]:[<code>|other|none]``.

  These will eventually replace the SEARCH_COUNTS - SERP results.

  They are broken down by the originating SAP where known:

  - ``urlbar``  Except search mode.
  - ``urlbar_handoff`` Used when searching from about:newtab.
  - ``urlbar_searchmode``  Used when the Urlbar is in search mode.
  - ``searchbar``
  - ``about_home``
  - ``about_newtab``
  - ``contextmenu``
  - ``webextension``
  - ``system`` Indicates a search from the command line.
  - ``tabhistory`` Indicates a search was counted as a result of the user loading it from the tab history.
  - ``reload`` Indicates a search was counted as a result of reloading the page.
  - ``unknown`` Indicates the origin was unknown.

browser.search.withads.*
  These keyed scalar track counts of SERP pages with adverts displayed. The key
  format is ``<provider>:<tagged|organic>``.

  They are broken down by the originating SAP where known, the list of SAP
  is the same as for ``browser.search.content.*``.

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

.. _additional telemetry of its own: /browser/urlbar/telemetry.html
.. _SearchSERPTelemetry.jsm and the associated parent/child actors: https://searchfox.org/mozilla-central/search?q=&path=SearchSERPTelemetry*.jsm&case=false&regexp=false
.. _BrowserSearchTelemetry.jsm: https://searchfox.org/mozilla-central/source/browser/components/search/BrowserSearchTelemetry.jsm
