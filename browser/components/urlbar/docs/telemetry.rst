Telemetry
=========

This section describes existing telemetry probes measuring interaction with the
Address Bar.

For telemetry specific to Firefox Suggest, see the
:doc:`firefox-suggest-telemetry` document.

.. contents::
   :depth: 2


Histograms
----------

PLACES_AUTOCOMPLETE_1ST_RESULT_TIME_MS
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  This probe tracks the amount of time it takes to get the first result.
  It is an exponential histogram with values between 5 and 100.

PLACES_AUTOCOMPLETE_6_FIRST_RESULTS_TIME_MS
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  This probe tracks the amount of time it takes to get the first six results.
  It is an exponential histogram with values between 50 and 1000.

FX_URLBAR_SELECTED_RESULT_METHOD
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  This probe tracks how a result was picked by the user from the list.
  It is a categorical histogram with these values:

  - ``enter``
    The user pressed Enter without selecting a result first.
    This most likely happens when the user confirms the default preselected
    result (aka *heuristic result*), or when they select with the keyboard a
    one-off search button and confirm with Enter.
  - ``enterSelection``
    The user selected a result, but not using Tab or the arrow keys, and then
    pressed Enter. This is a rare and generally unexpected event, there may be
    exotic ways to select a result we didn't consider, that are tracked here.
    Look at arrowEnterSelection and tabEnterSelection for more common actions.
  - ``click``
    The user clicked on a result.
  - ``arrowEnterSelection``
    The user selected a result using the arrow keys, and then pressed Enter.
  - ``tabEnterSelection``
    The first key the user pressed to select a result was the Tab key, and then
    they pressed Enter. Note that this means the user could have used the arrow
    keys after first pressing the Tab key.
  - ``rightClickEnter``
    Before QuantumBar, it was possible to right-click a result to highlight but
    not pick it. Then the user could press Enter. This is no more possible.

FX_URLBAR_ZERO_PREFIX_DWELL_TIME_MS
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  This probe records the amount of time the zero-prefix view was shown; that is,
  the time from when it was opened to the time it was closed. "Zero-prefix"
  means the search string was empty, so the zero-prefix view is the view that's
  shown when the user clicks in the urlbar before typing a search string. Often
  it's also called the "top sites" view since normally it shows the user's top
  sites. This is an exponential histogram whose values range from 0 to 60,000
  with 50 buckets. Values are in milliseconds. This histogram was introduced in
  Firefox 110.0 in bug 1806765.

PLACES_FRECENCY_RECALC_CHUNK_TIME_MS
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  This records the time necessary to recalculate frecency of a chunk of pages,
  as defined in the `PlacesFrecencyRecalculator <https://searchfox.org/mozilla-central/source/toolkit/components/places/PlacesFrecencyRecalculator.sys.mjs>`_ module.

Scalars
-------

urlbar.abandonment
~~~~~~~~~~~~~~~~~~

  A uint recording the number of abandoned engagements in the urlbar. An
  abandonment occurs when the user begins using the urlbar but stops before
  completing the engagement. This can happen when the user clicks outside the
  urlbar to focus a different part of the window. It can also happen when the
  user switches to another window while the urlbar is focused.

urlbar.autofill_deletion
~~~~~~~~~~~~~~~~~~~~~~~~

  A uint recording the deletion count for autofilled string in the urlbar.
  This occurs when the user deletes whole autofilled string by BACKSPACE or
  DELETE key while the autofilled string is selected.

urlbar.engagement
~~~~~~~~~~~~~~~~~

  A uint recording the number of engagements the user completes in the urlbar.
  An engagement occurs when the user navigates to a page using the urlbar, for
  example by picking a result in the urlbar panel or typing a search term or URL
  in the urlbar and pressing the enter key.

urlbar.impression.*
~~~~~~~~~~~~~~~~~~~

  A uint recording the number of impression that was displaying when user picks
  any result.

  - ``autofill_about``
    For about-page type autofill.
  - ``autofill_adaptive``
    For adaptive history type autofill.
  - ``autofill_origin``
    For origin type autofill.
  - ``autofill_other``
    Counts how many times some other type of autofill result that does not have
    a specific scalar was shown. This is a fallback that is used when the code is
    not properly setting a specific autofill type, and it should not normally be
    used. If it appears in the data, it means we need to investigate and fix the
    code that is not properly setting a specific autofill type.
  - ``autofill_url``
    For url type autofill.

urlbar.persistedsearchterms.revert_by_popup_count
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  A uint that is incremented when search terms are persisted in the Urlbar and
  the Urlbar is reverted to show a full URL due to a PopupNotification. This
  can happen when a user is on a SERP and permissions are requested, e.g.
  request access to location. If the popup is persistent and the user did not
  dismiss it before switching tabs, the popup will reappear when they return to
  the tab. Thus, when returning to the tab with the persistent popup, this
  value will be incremented because it should have persisted search terms but
  instead showed a full URL.

urlbar.persistedsearchterms.view_count
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  A uint that is incremented when search terms should be persisted in the
  Urlbar. This will trigger when a user loads a SERP from any SAP that results
  in the search terms persisting in the Urlbar, as well as switching to a tab
  containing a SERP that should be persisting the search terms in the Urlbar,
  regardless of whether a PopupNotification is present. Thus, for every
  ``revert_by_popup_count``, there should be at least one corresponding
  ``view_count``.

urlbar.tips
~~~~~~~~~~~

  This is a keyed scalar whose values are uints and are incremented each time a
  tip result is shown, a tip is picked, and a tip's help button is picked. The
  keys are:

  - ``intervention_clear-help``
    Incremented when the user picks the help button in the clear-history search
    intervention.
  - ``intervention_clear-picked``
    Incremented when the user picks the clear-history search intervention.
  - ``intervention_clear-shown``
    Incremented when the clear-history search intervention is shown.
  - ``intervention_refresh-help``
    Incremented when the user picks the help button in the refresh-Firefox
    search intervention.
  - ``intervention_refresh-picked``
    Incremented when the user picks the refresh-Firefox search intervention.
  - ``intervention_refresh-shown``
    Incremented when the refresh-Firefox search intervention is shown.
  - ``intervention_update_ask-help``
    Incremented when the user picks the help button in the update_ask search
    intervention, which is shown when there's a Firefox update available but the
    user's preference says we should ask them to download and apply it.
  - ``intervention_update_ask-picked``
    Incremented when the user picks the update_ask search intervention.
  - ``intervention_update_ask-shown``
    Incremented when the update_ask search intervention is shown.
  - ``intervention_update_refresh-help``
    Incremented when the user picks the help button in the update_refresh search
    intervention, which is shown when the user's browser is up to date but they
    triggered the update intervention. We show this special refresh intervention
    instead.
  - ``intervention_update_refresh-picked``
    Incremented when the user picks the update_refresh search intervention.
  - ``intervention_update_refresh-shown``
    Incremented when the update_refresh search intervention is shown.
  - ``intervention_update_restart-help``
    Incremented when the user picks the help button in the update_restart search
    intervention, which is shown when there's an update and it's been downloaded
    and applied. The user needs to restart to finish.
  - ``intervention_update_restart-picked``
    Incremented when the user picks the update_restart search intervention.
  - ``intervention_update_restart-shown``
    Incremented when the update_restart search intervention is shown.
  - ``intervention_update_web-help``
    Incremented when the user picks the help button in the update_web search
    intervention, which is shown when we can't update the browser or possibly
    even check for updates for some reason, so the user should download the
    latest version from the web.
  - ``intervention_update_web-picked``
    Incremented when the user picks the update_web search intervention.
  - ``intervention_update_web-shown``
    Incremented when the update_web search intervention is shown.
  - ``tabtosearch-shown``
    Increment when a non-onboarding tab-to-search result is shown, once per
    engine per engagement. Please note that the number of times non-onboarding
    tab-to-search results are picked is the sum of all keys in
    ``urlbar.searchmode.tabtosearch``. Please also note that more detailed
    telemetry is recorded about both onboarding and non-onboarding tab-to-search
    results in urlbar.tabtosearch.*. These probes in ``urlbar.tips`` are still
    recorded because ``urlbar.tabtosearch.*`` is not currently recorded
    in Release.
  - ``tabtosearch_onboard-shown``
    Incremented when a tab-to-search onboarding result is shown, once per engine
    per engagement. Please note that the number of times tab-to-search
    onboarding results are picked is the sum of all keys in
    ``urlbar.searchmode.tabtosearch_onboard``.
  - ``searchTip_onboard-picked``
    Incremented when the user picks the onboarding search tip.
  - ``searchTip_onboard-shown``
    Incremented when the onboarding search tip is shown.
  - ``searchTip_persist-picked``
    Incremented when the user picks the urlbar persisted search tip.
  - ``searchTip_persist-shown``
    Incremented when the url persisted search tip is shown.
  - ``searchTip_redirect-picked``
    Incremented when the user picks the redirect search tip.
  - ``searchTip_redirect-shown``
    Incremented when the redirect search tip is shown.

urlbar.searchmode.*
~~~~~~~~~~~~~~~~~~~

  This is a set of keyed scalars whose values are uints incremented each
  time search mode is entered in the Urlbar. The suffix on the scalar name
  describes how search mode was entered. Possibilities include:

  - ``bookmarkmenu``
    Used when the user selects the Search Bookmarks menu item in the Library
    menu.
  - ``handoff``
    Used when the user uses the search box on the new tab page and is handed off
    to the address bar. NOTE: This entry point was disabled from Firefox 88 to
    91. Starting with 91, it will appear but in low volume. Users must have
    searching in the Urlbar disabled to enter search mode via handoff.
  - ``keywordoffer``
    Used when the user selects a keyword offer result.
  - ``oneoff``
    Used when the user selects a one-off engine in the Urlbar.
  - ``shortcut``
    Used when the user enters search mode with a keyboard shortcut or menu bar
    item (e.g. ``Accel+K``).
  - ``tabmenu``
    Used when the user selects the Search Tabs menu item in the tab overflow
    menu.
  - ``tabtosearch``
    Used when the user selects a tab-to-search result. These results suggest a
    search engine when the search engine's domain is autofilled.
  - ``tabtosearch_onboard``
    Used when the user selects a tab-to-search onboarding result. These are
    shown the first few times the user encounters a tab-to-search result.
  - ``topsites_newtab``
    Used when the user selects a search shortcut Top Site from the New Tab Page.
  - ``topsites_urlbar``
    Used when the user selects a search shortcut Top Site from the Urlbar.
  - ``touchbar``
    Used when the user taps a search shortct on the Touch Bar, available on some
    Macs.
  - ``typed``
    Used when the user types an engine alias in the Urlbar.
  - ``historymenu``
    Used when the user selects the Search History menu item in a History
    menu.
  - ``other``
    Used as a catchall for other behaviour. We don't expect this scalar to hold
    any values. If it does, we need to correct an issue with search mode entry
    points.

  The keys for the scalars above are engine and source names. If the user enters
  a remote search mode with a built-in engine, we record the engine name. If the
  user enters a remote search mode with an engine they installed (e.g. via
  OpenSearch or a WebExtension), we record ``other`` (not to be confused with
  the ``urlbar.searchmode.other`` scalar above). If they enter a local search
  mode, we record the English name of the result source (e.g. "bookmarks",
  "history", "tabs"). Note that we slightly modify the engine name for some
  built-in engines: we flatten all localized Amazon sites (Amazon.com,
  Amazon.ca, Amazon.de, etc.) to "Amazon" and we flatten all localized
  Wikipedia sites (Wikipedia (en), Wikipedia (fr), etc.) to "Wikipedia". This
  is done to reduce the number of keys used by these scalars.

urlbar.picked.*
~~~~~~~~~~~~~~~

  This is a set of keyed scalars whose values are uints incremented each
  time a result is picked from the Urlbar. The suffix on the scalar name
  is the result type. The keys for the scalars above are the 0-based index of
  the result in the urlbar panel when it was picked.

  .. note::
    Available from Firefox 84 on. Use the *FX_URLBAR_SELECTED_** histograms in
    earlier versions.

  .. note::
    Firefox 102 deprecated ``autofill`` and added ``autofill_about``,
    ``autofill_adaptive``, ``autofill_origin``, ``autofill_other``,
    ``autofill_preloaded``, and ``autofill_url``. In Firefox 116,
    ``autofill_preloaded`` was removed.

  Valid result types are:

  - ``autofill``
    This scalar was deprecated in Firefox 102 and replaced with
    ``autofill_about``, ``autofill_adaptive``, ``autofill_origin``,
    ``autofill_other``, ``autofill_preloaded``, and ``autofill_url``. Previously
    it was recorded in each of the cases that the other scalars now cover.
  - ``autofill_about``
    An autofilled "about:" page URI (e.g., about:config). The user must first
    type "about:" to trigger this type of autofill.
  - ``autofill_adaptive``
    An autofilled URL from the user's adaptive history. This type of autofill
    differs from ``autofill_url`` in two ways: (1) It's based on the user's
    adaptive history, a particular type of history that associates the user's
    search string with the URL they pick in the address bar. (2) It autofills
    full URLs instead of "up to the next slash" partial URLs. For more
    information on this type of autofill, see this `adaptive history autofill
    document`_.
  - ``autofill_origin``
    An autofilled origin_ from the user's history. Typically "origin" means a
    domain or host name like "mozilla.org". Technically it can also include a
    URL scheme or protocol like "https" and a port number like ":8000". Firefox
    can autofill domain names by themselves, domain names with schemes, domain
    names with ports, and domain names with schemes and ports. All of these
    cases count as origin autofill. For more information, see this `adaptive
    history autofill document`_.
  - ``autofill_other``
    Counts how many times some other type of autofill result that does not have
    a specific keyed scalar was picked at a given index. This is a fallback that
    is used when the code is not properly setting a specific autofill type, and
    it should not normally be used. If it appears in the data, it means we need
    to investigate and fix the code that is not properly setting a specific
    autofill type.
  - ``autofill_url``
    An autofilled URL or partial URL from the user's history. Firefox autofills
    URLs "up to the next slash", so to trigger URL autofill, the user must first
    type a domain name (or trigger origin autofill) and then begin typing the
    rest of the URL (technically speaking, its path). As they continue typing,
    the URL will only be partially autofilled up to the next slash, or if there
    is no next slash, to the end of the URL. This allows the user to easily
    visit different subpaths of a domain. For more information, see this
    `adaptive history autofill document`_.
  - ``bookmark``
    A bookmarked URL.
  - ``bookmark_adaptive``
    A bookmarked URL retrieved from adaptive history.
  - ``clipboard``
    A URL retrieved from the system clipboard.
  - ``dynamic``
    A specially crafted result, often used in experiments when basic types are
    not flexible enough for a rich layout.
  - ``dynamic_wikipedia``
    A dynamic Wikipedia Firefox Suggest result.
  - ``extension``
    Added by an add-on through the omnibox WebExtension API.
  - ``formhistory``
    A search suggestion from previous search history.
  - ``history``
    A URL from history.
  - ``history_adaptive``
    A URL from history retrieved from adaptive history.
  - ``keyword``
    A bookmark keyword.
  - ``navigational``
    A navigational suggestion Firefox Suggest result.
  - ``quickaction``
    A QuickAction.
  - ``quicksuggest``
    A Firefox Suggest (a.k.a. quick suggest) suggestion.
  - ``remotetab``
    A tab synced from another device.
  - ``searchengine``
    A search result, but not a suggestion. May be the default search action
    or a search alias.
  - ``searchsuggestion``
    A remote search suggestion.
  - ``switchtab``
    An open tab.
  - ``tabtosearch``
    A tab to search result.
  - ``tip``
    A tip result.
  - ``topsite``
    An entry from top sites.
  - ``trending``
    A trending suggestion.
  - ``unknown``
    An unknown result type, a bug should be filed to figure out what it is.
  - ``visiturl``
    The user typed string can be directly visited.
  - ``weather``
    A Firefox Suggest weather suggestion.

  .. _adaptive history autofill document: https://docs.google.com/document/d/e/2PACX-1vRBLr_2dxus-aYhZRUkW9Q3B1K0uC-a0qQyE3kQDTU3pcNpDHb36-Pfo9fbETk89e7Jz4nkrqwRhi4j/pub
  .. _origin: https://html.spec.whatwg.org/multipage/origin.html#origin

urlbar.picked.searchmode.*
~~~~~~~~~~~~~~~~~~~~~~~~~~

  This is a set of keyed scalars whose values are uints incremented each time a
  result is picked from the Urlbar while the Urlbar is in search mode. The
  suffix on the scalar name is the search mode entry point. The keys for the
  scalars are the 0-based index of the result in the urlbar panel when it was
  picked.

  .. note::
    These scalars share elements of both ``urlbar.picked.*`` and
    ``urlbar.searchmode.*``. Scalar name suffixes are search mode entry points,
    like ``urlbar.searchmode.*``. The keys for these scalars are result indices,
    like ``urlbar.picked.*``.

  .. note::
    These data are a subset of the data recorded by ``urlbar.picked.*``. For
    example, if the user enters search mode by clicking a one-off then selects
    a Google search suggestion at index 2, we would record in **both**
    ``urlbar.picked.searchsuggestion`` and ``urlbar.picked.searchmode.oneoff``.

urlbar.tabtosearch.*
~~~~~~~~~~~~~~~~~~~~

  This is a set of keyed scalars whose values are uints incremented when a
  tab-to-search result is shown, once per engine per engagement. There are two
  sub-probes: ``urlbar.tabtosearch.impressions`` and
  ``urlbar.tabtosearch.impressions_onboarding``. The former records impressions
  of regular tab-to-search results and the latter records impressions of
  onboarding tab-to-search results. The key values are identical to those of the
  ``urlbar.searchmode.*`` probes: they are the names of the engines shown in the
  tab-to-search results. Engines that are not built in are grouped under the
  key ``other``.

  .. note::
    Due to the potentially sensitive nature of these data, they are currently
    collected only on pre-release version of Firefox. See bug 1686330.

urlbar.zeroprefix.abandonment
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  A uint recording the number of abandonments of the zero-prefix view.
  "Zero-prefix" means the search string was empty, so the zero-prefix view is
  the view that's shown when the user clicks in the urlbar before typing a
  search string. Often it's called the "top sites" view since normally it shows
  the user's top sites. "Abandonment" means the user opened the zero-prefix view
  but it was closed without the user picking a result inside it. This scalar was
  introduced in Firefox 110.0 in bug 1806765.

urlbar.zeroprefix.engagement
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  A uint recording the number of engagements in the zero-prefix view.
  "Zero-prefix" means the search string was empty, so the zero-prefix view is
  the view that's shown when the user clicks in the urlbar before typing a
  search string. Often it's called the "top sites" view since normally it shows
  the user's top sites. "Engagement" means the user picked a result inside the
  view. This scalar was introduced in Firefox 110.0 in bug 1806765.

urlbar.zeroprefix.exposure
~~~~~~~~~~~~~~~~~~~~~~~~~~

  A uint recording the number of times the user was exposed to the zero-prefix
  view; that is, the number of times it was shown. "Zero-prefix" means the
  search string was empty, so the zero-prefix view is the view that's shown when
  the user clicks in the urlbar before typing a search string. Often it's called
  the "top sites" view since normally it shows the user's top sites. This scalar
  was introduced in Firefox 110.0 in bug 1806765.

urlbar.quickaction.impression
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  A uint recording the number of times the user was shown a quickaction, the
  key is in the form $key-$n where $n is the number of characters the user typed
  in order for the suggestion to show. See bug 1806024.

urlbar.quickaction.picked
~~~~~~~~~~~~~~~~~~~~~~~~~

  A uint recording the number of times the user selected a quickaction, the
  key is in the form $key-$n where $n is the number of characters the user typed
  in order for the suggestion to show. See bug 1783155.

places.*
~~~~~~~~

  This is Places related telemetry.

  Valid result types are:

  - ``sponsored_visit_no_triggering_url``
    Number of sponsored visits that could not find their triggering URL in
    history. We expect this to be a small number just due to the navigation layer
    manipulating URLs. A large or growing value may be a concern.
  - ``pages_need_frecency_recalculation``
    Number of pages in need of a frecency recalculation. This number should
    remain small compared to the total number of pages in the database (see the
    `PLACES_PAGES_COUNT` histogram). It can be used to valuate the frequency
    and size of recalculations, for performance reasons.

Search Engagement Telemetry
---------------------------

The search engagement telemetry provided since Firefox 110 is is recorded using
Glean events. Because of the data size, these events are collected only for a
subset of the population, using the Glean Sampling feature. Please see the
following documents for the details.

  - `Engagement`_ :
    It is defined as a completed action in urlbar, where a user picked one of
    the results.
  - `Abandonment`_ :
    It is defined as an action where the user open the results but does not
    complete an engagement action, usually unfocusing the urlbar. This also
    happens when the user switches to another window, if the results popup was
    opening.
  - `Impression`_ :
    It is defined as an action where the results had been shown to the user for
    a while. In default, it will be recorded when the same results have been
    shown and 1 sec has elapsed. The interval value can be modified through the
    `browser.urlbar.searchEngagementTelemetry.pauseImpressionIntervalMs`
    preference.

.. _Engagement: https://dictionary.telemetry.mozilla.org/apps/firefox_desktop/metrics/urlbar_engagement
.. _Abandonment: https://dictionary.telemetry.mozilla.org/apps/firefox_desktop/metrics/urlbar_abandonment
.. _Impression: https://dictionary.telemetry.mozilla.org/apps/firefox_desktop/metrics/urlbar_impression


Custom pings for Contextual Services
------------------------------------

Contextual Services currently has two features involving the address bar, top
sites and Firefox Suggest. Top sites telemetry is sent in the `"top-sites" ping`_,
which is described in the linked Glean Dictionary page. For Firefox
Suggest, see the :doc:`firefox-suggest-telemetry` document.

    .. _"top-sites" ping: https://mozilla.github.io/glean/book/user/pings/custom.html

Changelog
  Firefox 122.0
    PingCentre-sent custom pings removed. [Bug `1868580`_]

  Firefox 116.0
    The "top-sites" ping is implemented. [Bug `1836283`_]

.. _1868580: https://bugzilla.mozilla.org/show_bug.cgi?id=1868580
.. _1836283: https://bugzilla.mozilla.org/show_bug.cgi?id=1836283


Other telemetry relevant to the Address Bar
-------------------------------------------

Search Telemetry
~~~~~~~~~~~~~~~~

  Some of the `search telemetry`_ is also relevant to the address bar.

contextual.services.topsites.*
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  These keyed scalars instrument the impressions and clicks for sponsored top
  sites in the urlbar.
  The key is a combination of the source and the placement of the top sites link
  (1-based) such as 'urlbar_1'. For each key, it records the counter of the
  impression or click.
  Note that these scalars are shared with the top sites on the newtab page.

Telemetry Environment
~~~~~~~~~~~~~~~~~~~~~

  The following preferences relevant to the address bar are recorded in
  :doc:`telemetry environment data </toolkit/components/telemetry/data/environment>`:

    - ``browser.search.suggest.enabled``: The global toggle for search
      suggestions everywhere in Firefox (search bar, urlbar, etc.). Defaults to
      true.
    - ``browser.urlbar.autoFill``: The global preference for whether autofill in
      the urlbar is enabled. When false, all types of autofill are disabled.
    - ``browser.urlbar.autoFill.adaptiveHistory.enabled``: True if adaptive
      history autofill in the urlbar is enabled.
    - ``browser.urlbar.suggest.searches``: True if search suggestions are
      enabled in the urlbar. Defaults to false.

Firefox Suggest
~~~~~~~~~~~~~~~

  Telemetry specific to Firefox Suggest is described in the
  :doc:`firefox-suggest-telemetry` document.

.. _search telemetry: /browser/search/telemetry.html
