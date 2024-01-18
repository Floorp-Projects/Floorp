Preferences
===========

This document describes Preferences affecting the Firefox's address bar.
Preferences that are generated and updated by code won't be described here.

User Exposed
------------
These preferences are exposed through the Firefox UI.

browser.urlbar.shortcuts.bookmarks (boolean, default: true)
  Whether to show the bookmark search shortcut button in the view.
  Can be controlled from Search Preferences.

browser.urlbar.shortcuts.tabs (boolean, default: true)
  Whether to show the tabs search shortcut button in the view.
  Can be controlled from Search Preferences.

browser.urlbar.shortcuts.history (boolean, default: true)
  Whether to show the history search shortcut button in the view.
  Can be controlled from Search Preferences.

browser.urlbar.showSearchSuggestionsFirst (boolean, default: true)
  Whether to show search suggestions before general results.
  Can be controlled from Search Preferences.

browser.urlbar.showSearchTerms.enabled (boolean, default: true)
  Whether to show the search term in the urlbar
  on a default search engine results page.
  Can be controlled from Search Preferences.

browser.urlbar.suggest.bookmark (boolean, default: true)
  Whether results will include the user's bookmarks.
  Can be controlled from Privacy Preferences.

browser.urlbar.suggest.history (boolean, default: true)
  Whether results will include the user's history.
  Can be controlled from Privacy Preferences.

browser.urlbar.suggest.openpage (boolean, default: true)
  Whether results will include switch-to-tab results.
  Can be controlled from Privacy Preferences.

browser.urlbar.suggest.quicksuggest.nonsponsored (boolean, default: false)
  If ``browser.urlbar.quicksuggest.enabled`` is true, this controls whether
  results will include non-sponsored quick suggest suggestions. Otherwise
  non-sponsored suggestions will not be shown.

browser.urlbar.suggest.quicksuggest.sponsored (boolean, default: false)
 If ``browser.urlbar.quicksuggest.enabled`` is true, this controls whether
  results will include sponsored quick suggest suggestions. Otherwise sponsored
  suggestions will not be shown.

browser.urlbar.suggest.searches (boolean, default: true)
  Whether results will include search suggestions.
  Can be controlled from Search Preferences.

browser.urlbar.suggest.engines (boolean, default: true)
  Whether results will include search engines (e.g. tab-to-search).
  Can be controlled from Privacy Preferences.

browser.urlbar.suggest.topsites (boolean, default: true)
  Whether results will include top sites and the view will open on focus.
  Can be controlled from Privacy Preferences.

browser.search.suggest.enabled (boolean, default: true)
  Whether search suggestions are enabled globally, including the separate search
  bar.
  Can be controlled from Search Preferences.

browser.search.suggest.enabled.private (boolean, default: false)
  When search suggestions are enabled, controls whether they are provided in
  Private Browsing windows.
  Can be controlled from Search Preferences.


Hidden
------
These preferences are normally hidden, and should not be used unless you really
know what you are doing.

browser.urlbar.accessibility.tabToSearch.announceResults (boolean: default: true)
  Whether we announce to screen readers when tab-to-search results are inserted.

browser.urlbar.addons.featureGate (boolean, default: false)
  Feature gate pref for add-on suggestions in the urlbar.

browser.urlbar.autoFill (boolean, default: true)
  Autofill is the the feature that automatically completes domains and URLs that
  the user has visited as the user is typing them in the urlbar textbox.

browser.urlbar.autoFill.adaptiveHistory.enabled (boolean, default: false)
  Whether adaptive history autofill feature is enabled.

browser.urlbar.autoFill.adaptiveHistory.useCountThreshold (float, default: 1.0)
  Threshold for use count of input history that we handle as adaptive history
  autofill. If the use count is this value or more, it will be a candidate.

browser.urlbar.autoFill.stddevMultiplier (float, default: 0.0)
  Affects the frecency threshold of the autofill algorithm.  The threshold is
  the mean of all origin frecencies, plus one standard deviation multiplied by
  this value.

browser.urlbar.ctrlCanonizesURLs (boolean, default: true)
  Whether using `ctrl` when hitting return/enter in the URL bar (or clicking
  'go') should prefix 'www.' and suffix browser.fixup.alternate.suffix to the
  user value prior to navigating.

browser.urlbar.decodeURLsOnCopy (boolean, default: false)
  Whether copying the entire URL from the location bar will put a human
  readable (percent-decoded) URL on the clipboard.

browser.urlbar.delay (number, default: 50)
  The amount of time (ms) to wait after the user has stopped typing before
  fetching certain results. Reducing this doesn't make the Address Bar faster,
  it will instead make it access the disk more heavily, and potentially make it
  slower. Certain results, like the heuristic, always skip this timer anyway.

browser.urlbar.dnsResolveSingleWordsAfterSearch (number, default: 0)
  Controls when to DNS resolve single word search strings, after they were
  searched for. If the string is resolved as a valid host, show a
  "Did you mean to go to 'host'" prompt.
  Set to 0. 0: Never resolve, 1: Use heuristics, 2. Always resolve.

browser.urlbar.extension.timeout (integer, default: 400)
  When sending events to extensions, they have this amount of time in
  milliseconds to respond before timing out. This affects the omnibox API.

browser.urlbar.filter.javascript (boolean, default: true)
  When true, `javascript:` URLs are not included in search results for safety
  reasons.

browser.urlbar.formatting.enabled (boolean, default: true)
  Applies URL highlighting and other styling to the text in the urlbar input
  field. This should usually be enabled for security reasons.

browser.urlbar.maxCharsForSearchSuggestions (integer, default: 100)
  As a user privacy measure, we don't fetch results from remote services for
  searches that start by pasting a string longer than this. The pref name
  indicates search suggestions, but this is used for all remote results.

browser.urlbar.maxHistoricalSearchSuggestions (integer, default: 2)
  The maximum number of form history results to include as search history.

browser.urlbar.maxRichResults (integer, default: 10)
  The maximum number of results in the urlbar popup.

browser.urlbar.merino.clientVariants (string, default: "")
  Comma separated list of client variants to send to send to Merino. See
  `Merino API docs <https://mozilla-services.github.io/merino/api.html#suggest>`_
  for more details. This is intended to be used by experiments, not directly set
  by users.

browser.urlbar.merino.providers (string, default: "")
  Comma-separated list of providers to request from the Merino server. Merino
  will return suggestions only for these providers. See `Merino API docs`_ for
  more details.

browser.urlbar.openintab (boolean, default: false)
  Whether address bar results should be opened in new tabs by default.

browser.urlbar.pocket.featureGate (boolean, default: false)
  Feature gate pref for Pocket suggestions in the urlbar.

browser.urlbar.quicksuggest.enabled (boolean, default: false)
  Whether the quick suggest feature is enabled, i.e., sponsored and recommended
  results related to the user's search string. This pref can be overridden by
  the ``quickSuggestEnabled`` Nimbus variable. If false, neither sponsored nor
  non-sponsored quick suggest results will be shown. If true, then we look at
  the individual prefs ``browser.urlbar.suggest.quicksuggest.nonsponsored`` and
  ``browser.urlbar.suggest.quicksuggest.sponsored``.

browser.urlbar.quicksuggest.dataCollection.enabled (boolean, default: false)
  Whether data collection is enabled for quick suggest results.

browser.urlbar.quicksuggest.shouldShowOnboardingDialog (boolean, default: true)
  Whether to show the quick suggest onboarding dialog.

browser.urlbar.richSuggestions.tail (boolean, default: true)
  If true, we show tail search suggestions when available.

browser.urlbar.searchTips.test.ignoreShowLimits (boolean, default: false)
  Disables checks that prevent search tips being shown, thus showing them every
  time the newtab page or the default search engine homepage is opened.
  This is useful for testing purposes.

browser.urlbar.speculativeConnect.enabled (boolean, default: true)
  Speculative connections allow to resolve domains pre-emptively when the user
  is likely to pick a result from the Address Bar. This allows for faster
  navigation.

browser.urlbar.sponsoredTopSites (boolean, default: false)
  Whether top sites may include sponsored ones.

browser.urlbar.suggest.addons (boolean, default: true)
  If ``browser.urlbar.addons.featureGate`` is true, this controls whether add-on
  suggestions are turned on. Otherwise they won't be shown.

browser.urlbar.suggest.pocket (boolean, default: true)
  If ``browser.urlbar.pocket.featureGate`` is true, this controls whether Pocket
  suggestions are turned on. Otherwise they won't be shown.

browser.urlbar.suggest.yelp (boolean, default: true)
  If ``browser.urlbar.yelp.featureGate`` is true, this controls whether Yelp
  suggestions are turned on. Otherwise they won't be shown.

browser.urlbar.switchTabs.adoptIntoActiveWindow (boolean, default: false)
  When using switch to tabs, if set to true this will move the tab into the
  active window, instead of just switching to it.

browser.urlbar.trimURLs (boolean, default: true)
  Clean-up URLs when showing them in the Address Bar.

keyword.enabled (boolean, default: true)
  By default, when the search string is not recognized as a potential url,
  search for it with the default search engine. If set to false any string will
  be handled as a potential URL, even if it's invalid.

browser.fixup.dns_first_for_single_words (boolean, default: false)
  If true, any single word search string will be sent to the DNS server before
  deciding whether to search or visit it. This may add a delay to the urlbar.


Experimental
------------
These preferences are experimental and not officially supported. They could be
removed at any time.

browser.urlbar.suggest.calculator (boolean, default: false)
  Whether results will include a calculator.

browser.urlbar.unitConversion.enabled (boolean, default: false)
  Whether unit conversion is enabled.

browser.urlbar.unitConversion.suggestedIndex (integer, default: 1)
  The index where we show unit conversion results.

browser.urlbar.experimental.expandTextOnFocus (boolean, default: false)
  Whether we expand the font size when the urlbar is focused.

browser.urlbar.experimental.searchButton (boolean, default: false)
  Whether to displays a permanent search button before the urlbar.

browser.urlbar.keepPanelOpenDuringImeComposition (boolean, default: false)
  Whether the results panel should be kept open during IME composition. The
  panel may overlap with the IME compositor panel.

browser.urlbar.restyleSearches (boolean, default: false)
  When true, URLs in the user's history that look like search result pages
  are restyled to look like search engine results instead of history results.

browser.urlbar.update2.emptySearchBehavior (integer, default: 0)
  Controls the empty search behavior in Search Mode: 0. Show nothing, 1. Show
  search history, 2. Show search and browsing history
