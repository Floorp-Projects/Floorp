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

FX_URLBAR_SELECTED_RESULT_INDEX
  This probe tracks the indexes of picked results in the results list.
  It's an enumerated histogram with 17 buckets.

FX_URLBAR_SELECTED_RESULT_TYPE_2
  This probe tracks the types of picked results.
  It's an enumerated histogram with 32 buckets.
  Values can be:

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

FX_URLBAR_SELECTED_RESULT_INDEX_BY_TYPE_2
  This probe tracks picked result type, for each one it tracks the index where
  it appeared.
  It's a keyed histogram where the keys are result types (see
  `UrlbarUtils.SELECTED_RESULT_TYPES`_). For each key, this records the indexes
  of picked results for that result type.

  .. _UrlbarUtils.SELECTED_RESULT_TYPES: https://searchfox.org/mozilla-central/search?q=symbol:UrlbarUtils%23SELECTED_RESULT_TYPES&redirect=false

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
  - ``searchTip_onboard-picked``
    Incremented when the user picks the onboarding search tip.
  - ``searchTip_onboard-shown``
    Incremented when the onboarding search tip is shown.
  - ``searchTip_redirect-picked``
    Incremented when the user picks the redirect search tip.
  - ``searchTip_redirect-shown``
    Incremented when the redirect search tip is shown.

urlbar.searchmode.*
  This is set of keyed scalars whose values are uints are are incremented each
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
    The user used Paste & Go feature. It is not the same as paste and Enter.
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
    It can be one of: ``none``, ``autofill``, ``visit``, ``bookmark``,
    ``history``, ``keyword``, ``search``, ``searchsuggestion``, ``switchtab``,
    ``remotetab``, ``extension``, ``oneoff``, ``keywordoffer``, ``canonized``,
    ``tip``, ``tiphelp``, ``formhistory``
  - ``selIndex``
    Index of the selected result in the urlbar panel, or -1 for no selection.
    There won't be a selection when a one-off button is the only selection, and
    for the ``paste_go`` or ``drop_go`` objects. There may also not be a
    selection if the system was busy and results arrived too late, then we
    directly decide whether to search or visit the given string without having
    a fully built result.
    This is only present for ``engagement`` events.

Search probes relevant to the Address Bar
-----------------------------------------

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
      82+, since urlbar-searchmode replaces it.
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
    - ``system``

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

Obsolete probes
---------------

Obsolete histograms
~~~~~~~~~~~~~~~~~~~

FX_URLBAR_SELECTED_RESULT_TYPE (OBSOLETE)
  This probe is obsolete and was replaced with
  ``FX_URLBAR_SELECTED_RESULT_TYPE_2`` in Firefox 78 in order to increase the
  number of buckets.

  This probe tracked the types of picked results.
  It's an enumerated histogram with 14 buckets.
  Values can be:

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

FX_URLBAR_SELECTED_RESULT_INDEX_BY_TYPE (OBSOLETE)
  This probe is obsolete and was replaced with
  ``FX_URLBAR_SELECTED_RESULT_INDEX_BY_TYPE_2`` in Firefox 78 in order to
  increase the number of buckets.

  This probe tracked picked result type, for each one it tracks the index where
  it appeared.
  It's a keyed histogram where the keys are result types (see
  `UrlbarUtils.SELECTED_RESULT_TYPES`_). For each key, this records the indexes
  of picked results for that result type.

  .. _UrlbarUtils.SELECTED_RESULT_TYPES: https://searchfox.org/mozilla-central/search?q=symbol:UrlbarUtils%23SELECTED_RESULT_TYPES&redirect=false

Obsolete search probes
----------------------

navigation.search (OBSOLETE)
  This is a legacy and disabled event telemetry that is currently under
  discussion for removal or modernization. It can't be enabled through a pref.
  it's more or less equivalent to browser.engagement.navigation, but can also
  report the picked search engine.
