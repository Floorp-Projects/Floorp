Telemetry
=========

This section describes existing telemetry probes measuring interaction with the
Address Bar.

.. toctree::
   :caption: Table of Contents

   telemetry

Histograms
----------

PLACES_AUTOCOMPLETE_1ST_RESULT_TIME_MS
  This probe tracks the amount of time it takes to get the first result.
  It is an exponential histogram with values between 5 and 100.

PLACES_AUTOCOMPLETE_6_FIRST_RESULTS_TIME_MS
  This probe tracks the amount of time it takes to get the first six results.
  It is an exponential histogram with values between 50 and 1000.

FX_URLBAR_SELECTED_RESULT_METHOD
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

Scalars
-------

urlbar.tips
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
  - ``searchTip_redirect-picked``
    Incremented when the user picks the redirect search tip.
  - ``searchTip_redirect-shown``
    Incremented when the redirect search tip is shown.

urlbar.searchmode.*
  This is a set of keyed scalars whose values are uints incremented each
  time search mode is entered in the Urlbar. The suffix on the scalar name
  describes how search mode was entered. Possibilities include:

  - ``bookmarkmenu``
    Used when the user selects the Search Bookmarks menu item in the Library
    menu.
  - ``handoff``
    Used when the user uses the search box on the new tab page and is handed off
    to the address bar.
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
  This is a set of keyed scalars whose values are uints incremented each
  time a result is picked from the Urlbar. The suffix on the scalar name
  is the result type. The keys for the scalars above are the 0-based index of
  the result in the urlbar panel when it was picked.

  .. note::
    Available from Firefox 84 on. Use the *FX_URLBAR_SELECTED_** histograms in
    earlier versions. See the `Obsolete probes`_ section below.

  Valid result types are:

  - ``autofill``
    An origin or a URL completed the user typed text inline.
  - ``bookmark``
    A bookmarked URL.
  - ``dynamic``
    A specially crafted result, often used in experiments when basic types are
    not flexible enough for a rich layout.
  - ``extension``
    Added by an add-on through the omnibox WebExtension API.
  - ``formhistory``
    A search suggestion from previous search history.
  - ``history``
    A URL from history.
  - ``keyword``
    A bookmark keyword.
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
  - ``unknown``
    An unknown result type, a bug should be filed to figure out what it is.
  - ``visiturl``
    The user typed string can be directly visited.

urlbar.picked.searchmode.*
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



Event Telemetry
---------------

The event telemetry is grouped under the ``urlbar`` category.

Event Method
  There are two methods to describe the interaction with the urlbar:

  - ``engagement``
    It is defined as a completed action in urlbar, where a user inserts text
    and executes one of the actions described in the Event Object.
  - ``abandonment``
    It is defined as an action where the user inserts text but does not
    complete an engagement action, usually unfocusing the urlbar. This also
    happens when the user switches to another window, regardless of urlbar
    focus.

Event Value
  This is how the user interaction started

  - ``typed``: The text was typed into the urlbar.
  - ``dropped``: The text was drag and dropped into the urlbar.
  - ``pasted``: The text was pasted into the urlbar.
  - ``topsites``: The user opened the urlbar view without typing, dropping,
    or pasting.
    In these cases, if the urlbar input is showing the URL of the loaded page
    and the user has not modified the input’s content, the urlbar views shows
    the user’s top sites. Otherwise, if the user had modified the input’s
    content, the urlbar view shows results based on what the user has typed.
    To tell whether top sites were shown, it's enough to check whether value is
    ``topsites``. To know whether the user actually picked a top site, check
    check that ``numChars`` == 0. If ``numChars`` > 0, the user initially opened
    top sites, but then they started typing and confirmed a different result.
  - ``returned``: The user abandoned a search, for example by switching to
    another tab/window, or focusing something else, then came back to it
    and continued. We consider a search continued if the user kept at least the
    first char of the original search string.
  - ``restarted``: The user abandoned a search, for example by switching to
    another tab/window, or focusing something else, then came back to it,
    cleared it and then typed a new string.

Event Object
  These describe actions in the urlbar:

  - ``click``
    The user clicked on a result.
  - ``enter``
    The user confirmed a result with Enter.
  - ``drop_go``
    The user dropped text on the input field.
  - ``paste_go``
    The user used Paste and Go feature. It is not the same as paste and Enter.
  - ``blur``
    The user unfocused the urlbar. This is only valid for ``abandonment``.

Event Extra
  This object contains additional information about the interaction.
  Extra is a key-value store, where all the keys and values are strings.

  - ``elapsed``
    Time in milliseconds from the initial interaction to an action.
  - ``numChars``
    Number of input characters the user typed or pasted at the time of
    submission.
  - ``numWords``
    Number of words in the input. The measurement is taken from a trimmed input
    split up by its spaces. This is not a perfect measurement, since it will
    return an incorrect value for languages that do not use spaces or URLs
    containing spaces in its query parameters, for example.
  - ``selType``
    The type of the selected result at the time of submission.
    This is only present for ``engagement`` events.
    It can be one of: ``none``, ``autofill``, ``visiturl``, ``bookmark``,
    ``history``, ``keyword``, ``searchengine``, ``searchsuggestion``,
    ``switchtab``, ``remotetab``, ``extension``, ``oneoff``, ``keywordoffer``,
    ``canonized``, ``tip``, ``tiphelp``, ``formhistory``, ``tabtosearch``,
    ``help``, ``unknown``
    In practice, ``tabtosearch`` should not appear in real event telemetry.
    Opening a tab-to-search result enters search mode and entering search mode
    does not currently mark the end of an engagement. It is noted here for
    completeness.
  - ``selIndex``
    Index of the selected result in the urlbar panel, or -1 for no selection.
    There won't be a selection when a one-off button is the only selection, and
    for the ``paste_go`` or ``drop_go`` objects. There may also not be a
    selection if the system was busy and results arrived too late, then we
    directly decide whether to search or visit the given string without having
    a fully built result.
    This is only present for ``engagement`` events.
  - ``provider``
    The name of the result provider for the selected result. Existing values
    are: ``HeuristicFallback``, ``Autofill``, ``UnifiedComplete``,
    ``TokenAliasEngines``, ``SearchSuggestions``, ``UrlbarProviderTopSites``.
    Values can also be defined by `URLBar provider experiments`_.

    .. _URLBar provider experiments: experiments.html#developing-address-bar-extensions


Custom pings for Contextual Services
------------------------------------

Contextual Services currently has two features running within the Urlbar: TopSites
and QuickSuggest. We send various pings as the `custom pings`_ to record the impressions
and clicks of these two features.

    .. _custom pings: https://docs.telemetry.mozilla.org/cookbooks/new_ping.html#sending-a-custom-ping

TopSites Impression
  This records an impression when a sponsored TopSite is shown.

  - ``context_id``
    A UUID representing this user. Note that it's not client_id, nor can it be used to link to a client_id.
  - ``tile_id``
    A unique identifier for the sponsored TopSite.
  - ``source``
    The browser location where the impression was displayed.
  - ``position``
    The placement of the TopSite (1-based).
  - ``advertiser``
    The Name of the advertiser.
  - ``reporting_url``
    The reporting URL of the sponsored TopSite, normally pointing to the ad partner's reporting endpoint.
  - ``version``
    Firefox version.
  - ``release_channel``
    Firefox release channel.
  - ``locale``
    User's current locale.

TopSites Click
  This records a click ping when a sponsored TopSite is clicked by the user.

  - ``context_id``
    A UUID representing this user. Note that it's not client_id, nor can it be used to link to a client_id.
  - ``tile_id``
    A unique identifier for the sponsored TopSite.
  - ``source``
    The browser location where the click was tirggered.
  - ``position``
    The placement of the TopSite (1-based).
  - ``advertiser``
    The Name of the advertiser.
  - ``reporting_url``
    The reporting URL of the sponsored TopSite, normally pointing to the ad partner's reporting endpoint.
  - ``version``
    Firefox version.
  - ``release_channel``
    Firefox release channel.
  - ``locale``
    User's current locale.

QuickSuggest Impression
  This records an impression when the following two conditions hold:
    - A user needs to complete the search action by picking a result from the Urlbar
    - There must be a QuickSuggest link shown at the end of that search action.
      No impression will be recorded for any QuickSuggest links that are shown
      during the user typing, only the last one (if any) counts

  Payload:

  - ``context_id``
    A UUID representing this user. Note that it's not client_id, nor can it be used to link to a client_id.
  - ``search_query``
    The exact search query typed in by the user.
  - ``matched_keywords``
    The matched keywords that leads to the QuickSuggest link.
  - ``is_clicked``
    Whether or not the use has clicked on the QuickSuggest link.
  - ``block_id``
    A unique identifier for a QuickSuggest link (a.k.a a keywords block).
  - ``position``
    The placement of the QuickSuggest link in the Urlbar (1-based).
  - ``advertiser``
    The Name of the advertiser.
  - ``reporting_url``
    The reporting URL of the QuickSuggest link, normally pointing to the ad partner's reporting endpoint.

QuickSuggest Click
  This records a click ping when a QuickSuggest link is clicked by the user.

  - ``context_id``
    A UUID representing this user. Note that it's not client_id, nor can it be
    used to link to a client_id.
  - ``advertiser``
    The Name of the advertiser.
  - ``block_id``
    A unique identifier for a QuickSuggest link (a.k.a a keywords block).
  - ``position``
    The placement of the QuickSuggest link in the Urlbar (1-based).
  - ``reporting_url``
    The reporting URL of the QuickSuggest link, normally pointing to the ad partner's reporting endpoint.


Other telemetry relevant to the Address Bar
-------------------------------------------

SEARCH_COUNTS
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
    - ``urlbar-searchmode`` Used when the Urlbar is in search mode.
    - ``webextension``
    - ``oneoff-urlbar``
    - ``oneoff-searchbar``
    - ``unknown`` This is actually the searchbar, when using the current engine
      one-off button.

browser.engagement.navigation.*
  These keyed scalars track search through different SAPs, for example the
  urlbar is tracked by ``browser.engagement.navigation.urlbar``.
  It counts loads triggered in a subsession from the specified SAP, broken down
  by the originating action.
  Possible SAPs are:

    - ``urlbar``  Except search mode.
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

contextual.services.topsites.*
  These keyed scalars instrument the impressions and clicks for sponsored TopSites
  in the urlbar.
  The key is a combination of the source and the placement of the TopSites link
  (1-based) such as 'urlbar_1'. For each key, it records the counter of the
  impression or click.
  Note that these scalars are shared with the TopSites on the newtab page.

contextual.services.quicksuggest.*
  These keyed scalars record impressions and clicks on Quick Suggest results,
  also called Firefox Suggest results, in the address bar. The keys for each
  scalar are the 1-based indexes of the Quick Suggest results, and the values
  are the number of impressions or clicks for the corresponding indexes. For
  example, for a Quick Suggest impression at 0-based index 9, the value for key
  ``10`` will be incremented in the
  ``contextual.services.quicksuggest.impression`` scalar.

  The keyed scalars are:

    - ``contextual.services.quicksuggest.impression``
      Incremented when a Quick Suggest result is shown in an address bar
      engagement where the user picks any result. The particular picked result
      doesn't matter, and it doesn't need to be the Quick Suggest result.
    - ``contextual.services.quicksuggest.click``
      Incremented when the user picks a Quick Suggest result (not including the
      help button).
    - ``contextual.services.quicksuggest.help``
      Incremented when the user picks the onboarding help button in a Quick
      Suggest result.

contextservices.quicksuggest
  This is event telemetry under the ``contextservices.quicksuggest`` category.
  It's enabled only when the ``browser.urlbar.quicksuggest.enabled`` pref is
  true. An event is recorded when the user toggles the
  ``browser.urlbar.suggest.quicksuggest`` pref, which corresponds to the
  checkbox in about:preferences#search labeled "Show Firefox Suggest in the
  address bar (suggested and sponsored results)". If the user never toggles
  the pref, then this event is never recorded.

  The full spec for this event is:

    - Category: ``contextservices.quicksuggest``
    - Method: ``enable_toggled``
    - Objects: ``enabled``, ``disabled`` -- ``enabled`` is recorded when the
      pref is flipped from false to true, and ``disabled`` is recorded when the
      pref is flipped from true to false.
    - Value: Not used
    - Extra: Not used

Obsolete probes
---------------

Obsolete histograms
~~~~~~~~~~~~~~~~~~~

FX_URLBAR_SELECTED_RESULT_INDEX (OBSOLETE)
  This probe tracked the indexes of picked results in the results list.
  It was an enumerated histogram with 17 buckets.

FX_URLBAR_SELECTED_RESULT_TYPE and FX_URLBAR_SELECTED_RESULT_TYPE_2 (from Firefox 78 on) (OBSOLETE)
  This probe tracked the types of picked results.
  It was an enumerated histogram with 17 buckets:

    0. autofill
    1. bookmark
    2. history
    3. keyword
    4. searchengine
    5. searchsuggestion
    6. switchtab
    7. tag
    8. visiturl
    9. remotetab
    10. extension
    11. preloaded-top-site
    12. tip
    13. topsite
    14. formhistory
    15. dynamic
    16. tabtosearch

FX_URLBAR_SELECTED_RESULT_INDEX_BY_TYPE and FX_URLBAR_SELECTED_RESULT_INDEX_BY_TYPE_2 (from Firefox 78 on) (OBSOLETE)
  This probe tracked picked result type, for each one it tracked the index where
  it appeared.
  It was a keyed histogram where the keys were result types (see
  FX_URLBAR_SELECTED_RESULT_TYPE above). For each key, this recorded the indexes
  of picked results for that result type.

Obsolete search probes
----------------------

navigation.search (OBSOLETE)
  This is a legacy and disabled event telemetry that is currently under
  discussion for removal or modernization. It can't be enabled through a pref.
  it's more or less equivalent to browser.engagement.navigation, but can also
  report the picked search engine.
