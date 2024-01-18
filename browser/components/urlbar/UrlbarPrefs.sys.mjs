/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module exports the UrlbarPrefs singleton, which manages preferences for
 * the urlbar. It also provides access to urlbar Nimbus variables as if they are
 * preferences, but only for variables with fallback prefs.
 */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  Region: "resource://gre/modules/Region.sys.mjs",
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

const PREF_URLBAR_BRANCH = "browser.urlbar.";

// Prefs are defined as [pref name, default value] or [pref name, [default
// value, type]].  In the former case, the getter method name is inferred from
// the typeof the default value.
//
// NOTE: Don't name prefs (relative to the `browser.urlbar` branch) the same as
// Nimbus urlbar features. Doing so would cause a name collision because pref
// names and Nimbus feature names are both kept as keys in UrlbarPref's map. For
// a list of Nimbus features, see toolkit/components/nimbus/FeatureManifest.yaml.
const PREF_URLBAR_DEFAULTS = new Map([
  // Whether we announce to screen readers when tab-to-search results are
  // inserted.
  ["accessibility.tabToSearch.announceResults", true],

  // Feature gate pref for addon suggestions in the urlbar.
  ["addons.featureGate", false],

  // The number of times the user has clicked the "Show less frequently" command
  // for addon suggestions.
  ["addons.showLessFrequentlyCount", 0],

  // "Autofill" is the name of the feature that automatically completes domains
  // and URLs that the user has visited as the user is typing them in the urlbar
  // textbox.  If false, autofill will be disabled.
  ["autoFill", true],

  // Whether enabling adaptive history autofill. This pref is a fallback for the
  // Nimbus variable `autoFillAdaptiveHistoryEnabled`.
  ["autoFill.adaptiveHistory.enabled", false],

  // Minimum char length of the user's search string to enable adaptive history
  // autofill. This pref is a fallback for the Nimbus variable
  // `autoFillAdaptiveHistoryMinCharsThreshold`.
  ["autoFill.adaptiveHistory.minCharsThreshold", 0],

  // Threshold for use count of input history that we handle as adaptive history
  // autofill. If the use count is this value or more, it will be a candidate.
  // Set the threshold to not be candidate the input history passed approximately
  // 30 days since user input it as the default.
  ["autoFill.adaptiveHistory.useCountThreshold", [0.47, "float"]],

  // Affects the frecency threshold of the autofill algorithm.  The threshold is
  // the mean of all origin frecencies plus one standard deviation multiplied by
  // this value.  See UrlbarProviderPlaces.
  ["autoFill.stddevMultiplier", [0.0, "float"]],

  // Whether to show a link for using the search functionality provided by the
  // active view if the the view utilizes OpenSearch.
  ["contextualSearch.enabled", false],

  // Whether using `ctrl` when hitting return/enter in the URL bar
  // (or clicking 'go') should prefix 'www.' and suffix
  // browser.fixup.alternate.suffix to the URL bar value prior to
  // navigating.
  ["ctrlCanonizesURLs", true],

  // Whether copying the entire URL from the location bar will put a human
  // readable (percent-decoded) URL on the clipboard.
  ["decodeURLsOnCopy", false],

  // The amount of time (ms) to wait after the user has stopped typing before
  // fetching results.  However, we ignore this for the very first result (the
  // "heuristic" result).  We fetch it as fast as possible.
  ["delay", 50],

  // Some performance tests disable this because extending the urlbar needs
  // layout information that we can't get before the first paint. (Or we could
  // but this would mean flushing layout.)
  ["disableExtendForTests", false],

  // Ensure we use trailing dots for DNS lookups for single words that could
  // be hosts.
  ["dnsResolveFullyQualifiedNames", true],

  // Controls when to DNS resolve single word search strings, after they were
  // searched for. If the string is resolved as a valid host, show a
  // "Did you mean to go to 'host'" prompt.
  // 0 - never resolve; 1 - use heuristics (default); 2 - always resolve
  ["dnsResolveSingleWordsAfterSearch", 0],

  // Whether we expand the font size when when the urlbar is
  // focused.
  ["experimental.expandTextOnFocus", false],

  // Whether the heuristic result is hidden.
  ["experimental.hideHeuristic", false],

  // Whether the urlbar displays a permanent search button.
  ["experimental.searchButton", false],

  // Comma-separated list of `source.providers` combinations, that are used to
  // determine if an exposure event should be fired. This can be set by a
  // Nimbus variable and is expected to be set via nimbus experiment
  // configuration.
  ["exposureResults", ""],

  // When we send events to (privileged) extensions (urlbar API), we wait this
  // amount of time in milliseconds for them to respond before timing out.
  ["extension.timeout", 400],

  // When we send events to extensions that use the omnibox API, we wait this
  // amount of time in milliseconds for them to respond before timing out.
  ["extension.omnibox.timeout", 3000],

  // When true, `javascript:` URLs are not included in search results.
  ["filter.javascript", true],

  // Applies URL highlighting and other styling to the text in the urlbar input.
  ["formatting.enabled", true],

  // Interval time until taking pause impression telemetry.
  ["searchEngagementTelemetry.pauseImpressionIntervalMs", 1000],

  // Boolean to determine if the providers defined in `exposureResults`
  // should be displayed in search results. This can be set by a
  // Nimbus variable and is expected to be set via nimbus experiment
  // configuration. For the control branch of an experiment this would be
  // false and true for the treatment.
  ["showExposureResults", false],

  // Whether Firefox Suggest group labels are shown in the urlbar view in en-*
  // locales. Labels are not shown in other locales but likely will be in the
  // future.
  ["groupLabels.enabled", true],

  // Whether the results panel should be kept open during IME composition.
  ["keepPanelOpenDuringImeComposition", false],

  // As a user privacy measure, don't fetch results from remote services for
  // searches that start by pasting a string longer than this. The pref name
  // indicates search suggestions, but this is used for all remote results.
  ["maxCharsForSearchSuggestions", 100],

  // The maximum number of form history results to include.
  ["maxHistoricalSearchSuggestions", 0],

  // The maximum number of results in the urlbar popup.
  ["maxRichResults", 10],

  // Feature gate pref for mdn suggestions in the urlbar.
  ["mdn.featureGate", true],

  // Comma-separated list of client variants to send to Merino
  ["merino.clientVariants", ""],

  // The Merino endpoint URL, not including parameters.
  ["merino.endpointURL", "https://merino.services.mozilla.com/api/v1/suggest"],

  // Comma-separated list of providers to request from Merino
  ["merino.providers", ""],

  // Timeout for Merino fetches (ms).
  ["merino.timeoutMs", 200],

  // Whether addresses and search results typed into the address bar
  // should be opened in new tabs by default.
  ["openintab", false],

  // Feature gate pref for Pocket suggestions in the urlbar.
  ["pocket.featureGate", false],

  // The number of times the user has clicked the "Show less frequently" command
  // for Pocket suggestions.
  ["pocket.showLessFrequentlyCount", 0],

  // When true, URLs in the user's history that look like search result pages
  // are styled to look like search engine results instead of the usual history
  // results.
  ["restyleSearches", false],

  // If true, we show tail suggestions when available.
  ["richSuggestions.tail", true],

  // Hidden pref. Disables checks that prevent search tips being shown, thus
  // showing them every time the newtab page or the default search engine
  // homepage is opened.
  ["searchTips.test.ignoreShowLimits", false],

  // Whether to show each local search shortcut button in the view.
  ["shortcuts.bookmarks", true],
  ["shortcuts.tabs", true],
  ["shortcuts.history", true],
  ["shortcuts.quickactions", false],

  // Whether to show search suggestions before general results.
  ["showSearchSuggestionsFirst", true],

  // Global toggle for whether the show search terms feature
  // can be used at all, and enabled/disabled by the user.
  ["showSearchTerms.featureGate", false],

  // If true, show the search term in the Urlbar while on
  // a default search engine results page.
  ["showSearchTerms.enabled", true],

  // Whether speculative connections should be enabled.
  ["speculativeConnect.enabled", true],

  // Whether results will include the user's bookmarks.
  ["suggest.bookmark", true],

  // Whether results will include a calculator.
  ["suggest.calculator", false],

  // Whether results will include clipboard results.
  ["suggest.clipboard", true],

  // Whether results will include search engines (e.g. tab-to-search).
  ["suggest.engines", true],

  // Whether results will include the user's history.
  ["suggest.history", true],

  // Whether results will include switch-to-tab results.
  ["suggest.openpage", true],

  // If `pocket.featureGate` is true, this controls whether Pocket suggestions
  // are turned on.
  ["suggest.pocket", true],

  // Whether results will include synced tab results. The syncing of open tabs
  // must also be enabled, from Sync preferences.
  ["suggest.remotetab", true],

  // Whether results will include QuickActions in the default search mode.
  ["suggest.quickactions", false],

  // Controls whether searching for open tabs returns tabs from any container
  // or only from the current container.
  ["switchTabs.searchAllContainers", false],

  // If disabled, QuickActions will not be included in either the default search
  // mode or the QuickActions search mode.
  ["quickactions.enabled", false],

  // Whether we show the Actions section in about:preferences.
  ["quickactions.showPrefs", false],

  // Whether we will match QuickActions within a phrase and not only a prefix.
  ["quickactions.matchInPhrase", true],

  // Show multiple actions in a random order.
  ["quickactions.randomOrderActions", false],

  // The minumum amount of characters required for the user to input before
  // matching actions. Setting this to 0 will show the actions in the
  // zero prefix state.
  ["quickactions.minimumSearchString", 3],

  // Whether results will include non-sponsored quick suggest suggestions.
  ["suggest.quicksuggest.nonsponsored", false],

  // Whether results will include sponsored quick suggest suggestions.
  ["suggest.quicksuggest.sponsored", false],

  // If `browser.urlbar.addons.featureGate` is true, this controls whether
  // addon suggestions are turned on.
  ["suggest.addons", true],

  // If `browser.urlbar.mdn.featureGate` is true, this controls whether
  // mdn suggestions are turned on.
  ["suggest.mdn", true],

  // Whether results will include search suggestions.
  ["suggest.searches", false],

  // Whether results will include top sites and the view will open on focus.
  ["suggest.topsites", true],

  // If `browser.urlbar.weather.featureGate` is true, this controls whether
  // weather suggestions are turned on.
  ["suggest.weather", true],

  // If `browser.urlbar.trending.featureGate` is true, this controls whether
  // trending suggestions are turned on.
  ["suggest.trending", true],

  // If `browser.urlbar.recentsearches.featureGate` is true, this controls whether
  // recentsearches are turned on.
  ["suggest.recentsearches", true],

  // If `browser.urlbar.yelp.featureGate` is true, this controls whether
  // Yelp suggestions are turned on.
  ["suggest.yelp", true],

  // Feature gate pref for Yelp suggestions in the urlbar.
  ["yelp.featureGate", false],

  // JSON'ed array of blocked quick suggest URL digests.
  ["quicksuggest.blockedDigests", ""],

  // Global toggle for whether the quick suggest feature is enabled, i.e.,
  // sponsored and recommended results related to the user's search string.
  ["quicksuggest.enabled", false],

  // Whether non-sponsored quick suggest results are subject to impression
  // frequency caps. This pref is a fallback for the Nimbus variable
  // `quickSuggestImpressionCapsNonSponsoredEnabled`.
  ["quicksuggest.impressionCaps.nonSponsoredEnabled", false],

  // Whether sponsored quick suggest results are subject to impression frequency
  // caps. This pref is a fallback for the Nimbus variable
  // `quickSuggestImpressionCapsSponsoredEnabled`.
  ["quicksuggest.impressionCaps.sponsoredEnabled", false],

  // JSON'ed object of quick suggest impression stats. Used for implementing
  // impression frequency caps for quick suggest suggestions.
  ["quicksuggest.impressionCaps.stats", ""],

  // The user's response to the Firefox Suggest online opt-in dialog.
  ["quicksuggest.onboardingDialogChoice", ""],

  // Whether the Firefox Suggest data collection opt-in result is enabled. If
  // true, this implicitly disables shouldShowOnboardingDialog.
  ["quicksuggest.contextualOptIn", false],

  // Controls which variant of the copy is used for the Firefox Suggest
  // contextual opt-in result.
  ["quicksuggest.contextualOptIn.sayHello", false],

  // Controls whether the Firefox Suggest contextual opt-in result appears at
  // the top of results or at the bottom, after one-off buttons.
  ["quicksuggest.contextualOptIn.topPosition", true],

  // The last time (as ISO string) the user dismissed the Firefox Suggest
  // contextual opt-in result.
  ["quicksuggest.contextualOptIn.lastDismissed", ""],

  // If the user has gone through a quick suggest prefs migration, then this
  // pref will have a user-branch value that records the latest prefs version.
  // Version changelog:
  //
  // 0: (Unversioned) When `suggest.quicksuggest` is false, all quick suggest
  //    results are disabled and `suggest.quicksuggest.sponsored` is ignored. To
  //    show sponsored suggestions, both prefs must be true.
  //
  // 1: `suggest.quicksuggest` is removed, `suggest.quicksuggest.nonsponsored`
  //    is introduced. `suggest.quicksuggest.nonsponsored` and
  //    `suggest.quicksuggest.sponsored` are independent:
  //    `suggest.quicksuggest.nonsponsored` controls non-sponsored results and
  //    `suggest.quicksuggest.sponsored` controls sponsored results.
  //    `quicksuggest.dataCollection.enabled` is introduced.
  //
  // 2: For online, the defaults for `suggest.quicksuggest.nonsponsored` and
  //    `suggest.quicksuggest.sponsored` are true. Previously they were false.
  ["quicksuggest.migrationVersion", 0],

  // Whether Firefox Suggest will use the new Rust backend instead of the
  // original JS backend.
  ["quicksuggest.rustEnabled", false],

  // The Suggest Rust backend will ingest remote settings every N seconds as
  // defined by this pref. Ingestion uses nsIUpdateTimerManager so the interval
  // will persist across app restarts. The default value is 24 hours, same as
  // the interval used by the desktop remote settings client.
  ["quicksuggest.rustIngestIntervalSeconds", 60 * 60 * 24],

  // The Firefox Suggest scenario in which the user is enrolled. This is set
  // when the scenario is updated (see `updateFirefoxSuggestScenario`) and is
  // not a pref the user should set. Once initialized, its value is one of:
  // "history", "offline", "online"
  ["quicksuggest.scenario", ""],

  // Whether the user has opted in to data collection for quick suggest.
  ["quicksuggest.dataCollection.enabled", false],

  // The version of dialog user saw.
  ["quicksuggest.onboardingDialogVersion", ""],

  // Whether to show the quick suggest onboarding dialog.
  ["quicksuggest.shouldShowOnboardingDialog", true],

  // Whether the user has seen the onboarding dialog.
  ["quicksuggest.showedOnboardingDialog", false],

  // Count the restarts before showing the onboarding dialog.
  ["quicksuggest.seenRestarts", 0],

  // Whether quick suggest results can be shown in position specified in the
  // suggestions.
  ["quicksuggest.allowPositionInSuggestions", true],

  // Allow the result menu button to be reached with the Tab key.
  ["resultMenu.keyboardAccessible", true],

  // When using switch to tabs, if set to true this will move the tab into the
  // active window.
  ["switchTabs.adoptIntoActiveWindow", false],

  // The number of remaining times the user can interact with tab-to-search
  // onboarding results before we stop showing them.
  ["tabToSearch.onboard.interactionsLeft", 3],

  // The number of times the user has been shown the onboarding search tip.
  ["tipShownCount.searchTip_onboard", 0],

  // The number of times the user has been shown the urlbar persisted search tip.
  ["tipShownCount.searchTip_persist", 0],

  // The number of times the user has been shown the redirect search tip.
  ["tipShownCount.searchTip_redirect", 0],

  // Remove redundant portions from URLs.
  ["trimURLs", true],

  // Remove 'https://' from url when urlbar is focused.
  ["trimHttps", true],

  // If true, top sites may include sponsored ones.
  ["sponsoredTopSites", false],

  // Whether unit conversion is enabled.
  ["unitConversion.enabled", false],

  // The index where we show unit conversion results.
  ["unitConversion.suggestedIndex", 1],

  // Controls the empty search behavior in Search Mode:
  //  0 - Show nothing
  //  1 - Show search history
  //  2 - Show search and browsing history
  ["update2.emptySearchBehavior", 0],

  // Feature gate pref for weather suggestions in the urlbar.
  ["weather.featureGate", false],

  // When false, the weather suggestion will not be fetched when a VPN is
  // detected. When true, it will be fetched anyway.
  ["weather.ignoreVPN", false],

  // The minimum prefix length of a weather keyword the user must type to
  // trigger the suggestion. 0 means the min length should be taken from Nimbus
  // or remote settings.
  ["weather.minKeywordLength", 0],

  // Feature gate pref for trending suggestions in the urlbar.
  ["trending.featureGate", false],

  // Whether to only show trending results when the urlbar is in search
  // mode or when the user initially opens the urlbar without selecting
  // an engine.
  ["trending.requireSearchMode", true],

  // The maximum number of trending results to show in search mode.
  ["trending.maxResultsSearchMode", 10],

  // The maximum number of trending results to show while not in search mode.
  ["trending.maxResultsNoSearchMode", 10],

  // Feature gate pref for rich suggestions being shown in the urlbar.
  ["richSuggestions.featureGate", true],

  // Feature gate pref for clipboard suggestions in the urlbar.
  ["clipboard.featureGate", true],

  // Feature gate pref for recent searches being shown in the urlbar.
  ["recentsearches.featureGate", false],

  // The maximum number of recent searches we will show.
  ["recentsearches.maxResults", 5],

  // We only show recent searches within the past 31 days by default.
  ["recentsearches.expirationDays", 31],
]);

const PREF_OTHER_DEFAULTS = new Map([
  ["browser.fixup.dns_first_for_single_words", false],
  ["browser.search.suggest.enabled", true],
  ["browser.search.suggest.enabled.private", false],
  ["browser.search.widget.inNavBar", false],
  ["keyword.enabled", true],
  ["ui.popup.disable_autohide", false],
]);

// Default values for Nimbus urlbar variables that do not have fallback prefs.
// Variables with fallback prefs do not need to be defined here because their
// defaults are the values of their fallbacks.
const NIMBUS_DEFAULTS = {
  addonsShowLessFrequentlyCap: 0,
  experimentType: "",
  pocketShowLessFrequentlyCap: 0,
  quickSuggestRemoteSettingsDataType: "data",
  quickSuggestScoreMap: null,
  recordNavigationalSuggestionTelemetry: false,
  weatherKeywords: null,
  weatherKeywordsMinimumLength: 0,
  weatherKeywordsMinimumLengthCap: 0,
};

// Maps preferences under browser.urlbar.suggest to behavior names, as defined
// in mozIPlacesAutoComplete.
const SUGGEST_PREF_TO_BEHAVIOR = {
  history: "history",
  bookmark: "bookmark",
  openpage: "openpage",
  searches: "search",
};

const PREF_TYPES = new Map([
  ["boolean", "Bool"],
  ["float", "Float"],
  ["number", "Int"],
  ["string", "Char"],
]);

/**
 * Builds the standard result groups and returns the root group.  Result
 * groups determine the composition of results in the muxer, i.e., how they're
 * grouped and sorted.  Each group is an object that looks like this:
 *
 * {
 *   {UrlbarUtils.RESULT_GROUP} [group]
 *     This is defined only on groups without children, and it determines the
 *     result group that the group will contain.
 *   {number} [maxResultCount]
 *     An optional maximum number of results the group can contain.  If it's
 *     not defined and the parent group does not define `flexChildren: true`,
 *     then the max is the parent's max.  If the parent group defines
 *     `flexChildren: true`, then `maxResultCount` is ignored.
 *   {boolean} [flexChildren]
 *     If true, then child groups are "flexed", similar to flex in HTML.  Each
 *     child group should define the `flex` property (or, if they don't, `flex`
 *     is assumed to be zero).  `flex` is a number that defines the ratio of a
 *     child's result count to the total result count of all children.  More
 *     specifically, `flex: X` on a child means that the initial maximum result
 *     count of the child is `parentMaxResultCount * (X / N)`, where `N` is the
 *     sum of the `flex` values of all children.  If there are any child groups
 *     that cannot be completely filled, then the muxer will attempt to overfill
 *     the children that were completely filled, while still respecting their
 *     relative `flex` values.
 *   {number} [flex]
 *     The flex value of the group.  This should be defined only on groups
 *     where the parent defines `flexChildren: true`.  See `flexChildren` for a
 *     discussion of flex.
 *   {array} [children]
 *     An array of child group objects.
 * }
 *
 * @param {boolean} showSearchSuggestionsFirst
 *   If true, the suggestions group will come before the general group.
 * @returns {object}
 *   The root group.
 */
function makeResultGroups({ showSearchSuggestionsFirst }) {
  let rootGroup = {
    children: [
      // heuristic
      {
        maxResultCount: 1,
        children: [
          { group: lazy.UrlbarUtils.RESULT_GROUP.HEURISTIC_TEST },
          { group: lazy.UrlbarUtils.RESULT_GROUP.HEURISTIC_EXTENSION },
          { group: lazy.UrlbarUtils.RESULT_GROUP.HEURISTIC_SEARCH_TIP },
          { group: lazy.UrlbarUtils.RESULT_GROUP.HEURISTIC_OMNIBOX },
          { group: lazy.UrlbarUtils.RESULT_GROUP.HEURISTIC_ENGINE_ALIAS },
          { group: lazy.UrlbarUtils.RESULT_GROUP.HEURISTIC_BOOKMARK_KEYWORD },
          { group: lazy.UrlbarUtils.RESULT_GROUP.HEURISTIC_AUTOFILL },
          { group: lazy.UrlbarUtils.RESULT_GROUP.HEURISTIC_TOKEN_ALIAS_ENGINE },
          { group: lazy.UrlbarUtils.RESULT_GROUP.HEURISTIC_HISTORY_URL },
          { group: lazy.UrlbarUtils.RESULT_GROUP.HEURISTIC_FALLBACK },
        ],
      },
      // extensions using the omnibox API
      {
        group: lazy.UrlbarUtils.RESULT_GROUP.OMNIBOX,
      },
    ],
  };

  // Prepare the parent group for suggestions and general.
  let mainGroup = {
    flexChildren: true,
    children: [
      // suggestions
      {
        children: [
          {
            flexChildren: true,
            children: [
              {
                // If `maxHistoricalSearchSuggestions` == 0, the muxer forces
                // `maxResultCount` to be zero and flex is ignored, per query.
                flex: 2,
                group: lazy.UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
              },
              {
                flex: 2,
                group: lazy.UrlbarUtils.RESULT_GROUP.RECENT_SEARCH,
              },

              {
                flex: 4,
                group: lazy.UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
              },
            ],
          },
          {
            group: lazy.UrlbarUtils.RESULT_GROUP.TAIL_SUGGESTION,
          },
        ],
      },
      // general
      {
        group: lazy.UrlbarUtils.RESULT_GROUP.GENERAL_PARENT,
        children: [
          {
            availableSpan: 3,
            group: lazy.UrlbarUtils.RESULT_GROUP.INPUT_HISTORY,
          },
          {
            flexChildren: true,
            children: [
              {
                flex: 1,
                group: lazy.UrlbarUtils.RESULT_GROUP.REMOTE_TAB,
              },
              {
                flex: 2,
                group: lazy.UrlbarUtils.RESULT_GROUP.GENERAL,
              },
              {
                // We show relatively many about-page results because they're
                // only added for queries starting with "about:".
                flex: 2,
                group: lazy.UrlbarUtils.RESULT_GROUP.ABOUT_PAGES,
              },
            ],
          },
          {
            group: lazy.UrlbarUtils.RESULT_GROUP.INPUT_HISTORY,
          },
        ],
      },
    ],
  };
  if (!showSearchSuggestionsFirst) {
    mainGroup.children.reverse();
  }
  mainGroup.children[0].flex = 2;
  mainGroup.children[1].flex = 1;
  rootGroup.children.push(mainGroup);

  return rootGroup;
}

/**
 * Preferences class.  The exported object is a singleton instance.
 */
class Preferences {
  /**
   * Constructor
   */
  constructor() {
    this._map = new Map();
    this.QueryInterface = ChromeUtils.generateQI([
      "nsIObserver",
      "nsISupportsWeakReference",
    ]);

    Services.prefs.addObserver(PREF_URLBAR_BRANCH, this, true);
    for (let pref of PREF_OTHER_DEFAULTS.keys()) {
      Services.prefs.addObserver(pref, this, true);
    }
    this._observerWeakRefs = [];
    this.addObserver(this);

    // These prefs control the value of the shouldHandOffToSearchMode pref. They
    // are exposed as a class variable so UrlbarPrefs observers can watch for
    // changes in these prefs.
    this.shouldHandOffToSearchModePrefs = [
      "keyword.enabled",
      "suggest.searches",
    ];

    // This is resolved when the first update to the Firefox Suggest scenario
    // (on startup) finishes.
    this._firefoxSuggestScenarioStartupPromise = new Promise(
      resolve => (this._resolveFirefoxSuggestScenarioStartupPromise = resolve)
    );

    // This is set to true when we update the Firefox Suggest scenario to
    // prevent re-entry due to pref observers.
    this._updatingFirefoxSuggestScenario = false;

    lazy.NimbusFeatures.urlbar.onUpdate(() => this._onNimbusUpdate());
  }

  /**
   * Returns the value for the preference with the given name.
   * For preferences in the "browser.urlbar."" branch, the passed-in name
   * should be relative to the branch. It's also possible to get prefs from the
   * PREF_OTHER_DEFAULTS Map, specifying their full name.
   *
   * @param {string} pref
   *        The name of the preference to get.
   * @returns {*} The preference value.
   */
  get(pref) {
    let value = this._map.get(pref);
    if (value === undefined) {
      value = this._getPrefValue(pref);
      this._map.set(pref, value);
    }
    return value;
  }

  /**
   * Sets the value for the preference with the given name.
   * For preferences in the "browser.urlbar."" branch, the passed-in name
   * should be relative to the branch. It's also possible to set prefs from the
   * PREF_OTHER_DEFAULTS Map, specifying their full name.
   *
   * @param {string} pref
   *        The name of the preference to set.
   * @param {*} value The preference value.
   */
  set(pref, value) {
    let { defaultValue, set } = this._getPrefDescriptor(pref);
    if (typeof value != typeof defaultValue) {
      throw new Error(`Invalid value type ${typeof value} for pref ${pref}`);
    }
    set(pref, value);
  }

  /**
   * Clears the value for the preference with the given name.
   *
   * @param {string} pref
   *        The name of the preference to clear.
   */
  clear(pref) {
    let { clear } = this._getPrefDescriptor(pref);
    clear(pref);
  }

  /**
   * Builds the standard result groups.  See makeResultGroups.
   *
   * @param {object} options
   *   See makeResultGroups.
   * @returns {object}
   *   The root group.
   */
  makeResultGroups(options) {
    return makeResultGroups(options);
  }

  get resultGroups() {
    if (!this.#resultGroups) {
      this.#resultGroups = makeResultGroups({
        showSearchSuggestionsFirst: this.get("showSearchSuggestionsFirst"),
      });
    }
    return this.#resultGroups;
  }

  /**
   * Sets the appropriate Firefox Suggest scenario based on the current Nimbus
   * rollout (if any) and "hardcoded" rollouts (if any). The possible scenarios
   * are:
   *
   * history
   *   This is the scenario when the user is not in any rollouts. Firefox
   *   Suggest suggestions are disabled.
   * offline
   *   This is the scenario for the "offline" rollout. Firefox Suggest
   *   suggestions are enabled by default. Data collection is not enabled by
   *   default, but the user can opt in in about:preferences. The onboarding
   *   dialog is not shown.
   * online
   *   This is the scenario for the "online" rollout. Firefox Suggest
   *   suggestions are enabled by default. Data collection is not enabled by
   *   default, and the user will be shown an onboarding dialog that prompts
   *   them to opt in to it. The user can also opt in in about:preferences.
   *
   * @param {string} [testOverrides]
   *   This is intended for tests only. Pass to force the following:
   *   `{ scenario, migrationVersion, defaultPrefs, isStartup }`
   */
  async updateFirefoxSuggestScenario(testOverrides = null) {
    // Make sure we don't re-enter this method while updating prefs. Updates to
    // prefs that are fallbacks for Nimbus variables trigger the pref observer
    // in Nimbus, which triggers our Nimbus `onUpdate` callback, which calls
    // this method again.
    if (this._updatingFirefoxSuggestScenario) {
      return;
    }

    let isStartup =
      !this._updateFirefoxSuggestScenarioCalled || !!testOverrides?.isStartup;
    this._updateFirefoxSuggestScenarioCalled = true;

    try {
      this._updatingFirefoxSuggestScenario = true;

      // This is called early in startup by BrowserGlue, so make sure the user's
      // region and our Nimbus variables are initialized since the scenario may
      // depend on them. Also note that pref migrations may depend on the
      // scenario, and since each migration is performed only once, at startup,
      // prefs can end up wrong if their migrations use the wrong scenario.
      await lazy.Region.init();
      await lazy.NimbusFeatures.urlbar.ready();
      this._clearNimbusCache();

      // This also races TelemetryEnvironment's initialization, so wait for it
      // to finish. TelemetryEnvironment is important because it records the
      // values of a number of Suggest preferences. If we didn't wait, we could
      // end up updating prefs after TelemetryEnvironment does its initial pref
      // cache but before it adds its observer to be notified of pref changes.
      // It would end up recording the wrong values on startup in that case.
      if (!this._testSkipTelemetryEnvironmentInit) {
        await lazy.TelemetryEnvironment.onInitialized();
      }

      this._updateFirefoxSuggestScenarioHelper(isStartup, testOverrides);
    } finally {
      this._updatingFirefoxSuggestScenario = false;
    }

    // Null check `_resolveFirefoxSuggestScenarioStartupPromise` since some
    // tests force `isStartup` after actual startup and it's been set to null.
    if (isStartup && this._resolveFirefoxSuggestScenarioStartupPromise) {
      this._resolveFirefoxSuggestScenarioStartupPromise();
      this._resolveFirefoxSuggestScenarioStartupPromise = null;
    }
  }

  _updateFirefoxSuggestScenarioHelper(isStartup, testOverrides) {
    // Updating the scenario is tricky and it's important to preserve the user's
    // choices, so we describe the process in detail below. tl;dr:
    //
    // * Prefs exposed in the UI should be sticky.
    // * Prefs that are both exposed in the UI and configurable via Nimbus
    //   should be added to `uiPrefNamesByVariable` below.
    // * Prefs that are both exposed in the UI and configurable via Nimbus don't
    //   need to be specified as a `fallbackPref` in the feature manifest.
    //   Access these prefs directly instead of through their Nimbus variables.
    // * If you are modifying this method, keep in mind that setting a pref
    //   that's a `fallbackPref` for a Nimbus variable will trigger the pref
    //   observer inside Nimbus and call all `NimbusFeatures.urlbar.onUpdate`
    //   callbacks. Inside this class we guard against that by using
    //   `updatingFirefoxSuggestScenario`.
    //
    // The scenario-update process is described next.
    //
    // 1. Pick a scenario. If the user is in a Nimbus rollout, then Nimbus will
    //    define it. Otherwise the user may be in a "hardcoded" rollout
    //    depending on their region and locale. If the user is not in any
    //    rollouts, then the scenario is "history", which means no Firefox
    //    Suggest suggestions should appear.
    //
    // 2. Set prefs on the default branch appropriate for the scenario. We use
    //    the default branch and not the user branch because conceptually each
    //    scenario has a default behavior, which we want to distinguish from the
    //    user's choices.
    //
    //    In particular it's important to consider prefs that are exposed in the
    //    UI, like whether sponsored suggestions are enabled. Once the user
    //    makes a choice to change a default, we want to preserve that choice
    //    indefinitely regardless of the scenario the user is currently enrolled
    //    in or future scenarios they might be enrolled in. User choices are of
    //    course recorded on the user branch, so if we set scenario defaults on
    //    the user branch too, we wouldn't be able to distinguish user choices
    //    from default values. This is also why prefs that are exposed in the UI
    //    should be sticky. Unlike non-sticky prefs, sticky prefs retain their
    //    user-branch values even when those values are the same as the ones on
    //    the default branch.
    //
    //    It's important to note that the defaults we set here do not persist
    //    across app restarts. (This is a feature of the pref service; prefs set
    //    programmatically on the default branch are not stored anywhere
    //    permanent like firefox.js or user.js.) That's why BrowserGlue calls
    //    `updateFirefoxSuggestScenario` on every startup.
    //
    // 3. Some prefs are both exposed in the UI and configurable via Nimbus,
    //    like whether data collection is enabled. We absolutely want to
    //    preserve the user's past choices for these prefs. But if the user
    //    hasn't yet made a choice for a particular pref, then it should be
    //    configurable.
    //
    //    For any such prefs that have values defined in Nimbus, we set their
    //    default-branch values to their Nimbus values. (These defaults
    //    therefore override any set in the previous step.) If a pref has a user
    //    value, accessing the pref will return the user value; if it does not
    //    have a user value, accessing it will return the value that was
    //    specified in Nimbus.
    //
    //    This isn't strictly necessary. Since prefs exposed in the UI are
    //    sticky, they will always preserve their user-branch values regardless
    //    of their default-branch values, and as long as a pref is listed as a
    //    `fallbackPref` for its corresponding Nimbus variable, Nimbus will use
    //    the user-branch value. So we could instead specify fallback prefs in
    //    Nimbus and always access values through Nimbus instead of through
    //    prefs. But that would make preferences UI code a little harder to
    //    write since the checked state of a checkbox would depend on something
    //    other than its pref. Since we're already setting default-branch values
    //    here as part of the previous step, it's not much more work to set
    //    defaults for these prefs too, and it makes the UI code a little nicer.
    //
    // 4. Migrate prefs as necessary. This refers to any pref changes that are
    //    neccesary across app versions: introducing and initializing new prefs,
    //    removing prefs, or changing the meaning of existing prefs.

    // 1. Pick a scenario
    let scenario =
      testOverrides?.scenario || this._getIntendedFirefoxSuggestScenario();

    // 2. Set default-branch values for the scenario
    let defaultPrefs =
      testOverrides?.defaultPrefs || this.FIREFOX_SUGGEST_DEFAULT_PREFS;
    let prefs = { ...defaultPrefs[scenario] };

    // 3. Set default-branch values for prefs that are both exposed in the UI
    // and configurable via Nimbus
    for (let [variable, prefName] of Object.entries(
      this.FIREFOX_SUGGEST_UI_PREFS_BY_VARIABLE
    )) {
      if (this._nimbus.hasOwnProperty(variable)) {
        prefs[prefName] = this._nimbus[variable];
      }
    }

    let defaults = Services.prefs.getDefaultBranch("browser.urlbar.");
    for (let [name, value] of Object.entries(prefs)) {
      // We assume all prefs are boolean right now.
      defaults.setBoolPref(name, value);
    }

    // 4. Migrate prefs across app versions
    if (isStartup) {
      this._ensureFirefoxSuggestPrefsMigrated(scenario, testOverrides);
    }

    // Set the scenario pref only after migrating so that migrations can tell
    // what the last-seen scenario was. Set it on the user branch so that its
    // value persists across app restarts.
    this.set("quicksuggest.scenario", scenario);
  }

  /**
   * Returns the Firefox Suggest scenario the user should be enrolled in. This
   * does *not* return the scenario they are currently enrolled in.
   *
   * @returns {string}
   *   The scenario the user should be enrolled in.
   */
  _getIntendedFirefoxSuggestScenario() {
    // If the user is in a Nimbus rollout, then Nimbus will define the scenario.
    // Otherwise the user may be in a "hardcoded" rollout depending on their
    // region and locale. If the user is not in any rollouts, then the scenario
    // is "history", which means no Firefox Suggest suggestions will appear.
    let scenario = this._nimbus.quickSuggestScenario;
    if (!scenario) {
      if (
        lazy.Region.home == "US" &&
        Services.locale.appLocaleAsBCP47.substring(0, 2) == "en"
      ) {
        // offline rollout for en locales in the US region
        scenario = "offline";
      } else {
        // no rollout
        scenario = "history";
      }
    }
    if (!this.FIREFOX_SUGGEST_DEFAULT_PREFS.hasOwnProperty(scenario)) {
      scenario = "history";
      console.error(`Unrecognized Firefox Suggest scenario "${scenario}"`);
    }
    return scenario;
  }

  /**
   * Prefs that are exposed in the UI and whose default-branch values are
   * configurable via Nimbus variables. This getter returns an object that maps
   * from variable names to pref names relative to `browser.urlbar`. See point 3
   * in the comment inside `_updateFirefoxSuggestScenarioHelper()` for more.
   *
   * @returns {{ quickSuggestNonSponsoredEnabled: string; quickSuggestSponsoredEnabled: string; quickSuggestDataCollectionEnabled: string; }}
   */
  get FIREFOX_SUGGEST_UI_PREFS_BY_VARIABLE() {
    return {
      quickSuggestNonSponsoredEnabled: "suggest.quicksuggest.nonsponsored",
      quickSuggestSponsoredEnabled: "suggest.quicksuggest.sponsored",
      quickSuggestDataCollectionEnabled: "quicksuggest.dataCollection.enabled",
    };
  }

  /**
   * Default prefs relative to `browser.urlbar` per Firefox Suggest scenario.
   *
   * @returns {Record<Record<string, boolean>>}
   */
  get FIREFOX_SUGGEST_DEFAULT_PREFS() {
    // Important notes when modifying this:
    //
    // If you add a pref to one scenario, you typically need to add it to all
    // scenarios even if the pref is in firefox.js. That's because we need to
    // allow for switching from one scenario to another at any time after
    // startup. If we set a pref for one scenario on the default branch, we
    // switch to a new scenario, and we don't set the pref for the new scenario,
    // it will keep its default-branch value from the old scenario. The only
    // possible exception is for prefs that make others unnecessary, like how
    // when `quicksuggest.enabled` is false, none of the other prefs matter.
    //
    // Prefs not listed here for any scenario keep their values set in
    // firefox.js.
    return {
      history: {
        "quicksuggest.enabled": false,
      },
      offline: {
        "quicksuggest.enabled": true,
        "quicksuggest.dataCollection.enabled": false,
        "quicksuggest.shouldShowOnboardingDialog": false,
        "suggest.quicksuggest.nonsponsored": true,
        "suggest.quicksuggest.sponsored": true,
      },
      online: {
        "quicksuggest.enabled": true,
        "quicksuggest.dataCollection.enabled": false,
        "quicksuggest.shouldShowOnboardingDialog": true,
        "suggest.quicksuggest.nonsponsored": true,
        "suggest.quicksuggest.sponsored": true,
      },
    };
  }

  /**
   * The current version of the Firefox Suggest prefs.
   *
   * @returns {number}
   */
  get FIREFOX_SUGGEST_MIGRATION_VERSION() {
    return 2;
  }

  /**
   * Migrates Firefox Suggest prefs to the current version if they haven't been
   * migrated already.
   *
   * @param {string} scenario
   *   The current Firefox Suggest scenario.
   * @param {string} testOverrides
   *   This is intended for tests only. Pass to force a migration version:
   *   `{ migrationVersion }`
   */
  _ensureFirefoxSuggestPrefsMigrated(scenario, testOverrides) {
    let currentVersion =
      testOverrides?.migrationVersion !== undefined
        ? testOverrides.migrationVersion
        : this.FIREFOX_SUGGEST_MIGRATION_VERSION;
    let lastSeenVersion = Math.max(
      0,
      this.get("quicksuggest.migrationVersion")
    );
    if (currentVersion <= lastSeenVersion) {
      // Migration up to date.
      return;
    }

    let version = lastSeenVersion;

    // When the current scenario is online and the last-seen prefs version is
    // unversioned, specially handle migration up to version 2.
    if (!version && scenario == "online" && 2 <= currentVersion) {
      this._migrateFirefoxSuggestPrefsUnversionedTo2Online();
      version = 2;
    }

    // Migrate from the last-seen version up to the current version.
    for (; version < currentVersion; version++) {
      let nextVersion = version + 1;
      let methodName = "_migrateFirefoxSuggestPrefsTo_" + nextVersion;
      try {
        this[methodName](scenario);
      } catch (error) {
        console.error(
          `Error migrating Firefox Suggest prefs to version ${nextVersion}:`,
          error
        );
        break;
      }
    }

    // Record the new last-seen migration version.
    this.set("quicksuggest.migrationVersion", version);
  }

  /**
   * Migrates unversioned Firefox Suggest prefs to version 2 but only when the
   * user's current scenario is online. This case requires special handling that
   * isn't covered by the usual migration path from unversioned to 2.
   */
  _migrateFirefoxSuggestPrefsUnversionedTo2Online() {
    // Copy `suggest.quicksuggest` to `suggest.quicksuggest.nonsponsored` and
    // clear the first.
    let mainPref = "browser.urlbar.suggest.quicksuggest";
    let mainPrefHasUserValue = Services.prefs.prefHasUserValue(mainPref);
    if (mainPrefHasUserValue) {
      this.set(
        "suggest.quicksuggest.nonsponsored",
        Services.prefs.getBoolPref(mainPref)
      );
      Services.prefs.clearUserPref(mainPref);
    }

    if (!this.get("quicksuggest.showedOnboardingDialog")) {
      // The user was enrolled in history or offline, or they were enrolled in
      // online and weren't shown the modal yet.
      //
      // If they were in history, they should now see suggestions by default,
      // and we don't need to worry about any current pref values since Firefox
      // Suggest is new to them.
      //
      // If they were in offline, they saw suggestions by default, but if they
      // disabled the main suggestions pref, then both non-sponsored and
      // sponsored suggestions were disabled and we need to carry that forward.
      //
      // If they were in online and weren't shown the modal yet, suggestions
      // were disabled by default. The modal is shown only on startup, so it's
      // possible they used Firefox for quite a while after being enrolled in
      // online with suggestions disabled the whole time. If they looked at the
      // prefs UI, they would have seen both suggestion checkboxes unchecked.
      // For these users, ideally we wouldn't suddenly enable suggestions, but
      // unfortunately there's no simple way to distinguish them from history
      // and offline users at this point based on the unversioned prefs. We
      // could check whether the user is or was enrolled in the initial online
      // experiment; if they were, then disable suggestions. However, that's a
      // little risky because it assumes future online rollouts will be
      // delivered by new experiments and not by increasing the original
      // experiment's population. If that assumption does not hold, we would end
      // up disabling suggestions for all users who are newly enrolled in online
      // even if they were previously in history or offline. Further, based on
      // telemetry data at the time of writing, only a small number of users in
      // online have not yet seen the modal. Therefore we will enable
      // suggestions for these users too.
      //
      // Note that if the user is in online and hasn't been shown the modal yet,
      // we'll show it at some point during startup right after this. However,
      // starting with the version-2 prefs, the modal now opts the user in to
      // only data collection, not suggestions as it previously did.

      if (
        mainPrefHasUserValue &&
        !this.get("suggest.quicksuggest.nonsponsored")
      ) {
        // The user was in offline and disabled the main suggestions pref, so
        // sponsored suggestions were automatically disabled too. We know they
        // disabled the main pref since it has a false user-branch value.
        this.set("suggest.quicksuggest.sponsored", false);
      }
      return;
    }

    // At this point, the user was in online, they were shown the modal, and the
    // current scenario is online. In the unversioned prefs for online, the
    // suggestion prefs were false on the default branch, but in the version-2
    // prefs, they're true on the default branch.

    if (mainPrefHasUserValue && this.get("suggest.quicksuggest.nonsponsored")) {
      // The main pref is true on the user branch. The user opted in either via
      // the modal or by checking the checkbox in the prefs UI. In the latter
      // case, they were shown some informational text about data collection
      // under the checkbox. Either way, they've opted in to data collection.
      this.set("quicksuggest.dataCollection.enabled", true);
      if (
        !Services.prefs.prefHasUserValue(
          "browser.urlbar.suggest.quicksuggest.sponsored"
        )
      ) {
        // The sponsored pref does not have a user value, so the default-branch
        // false value was the effective value and the user did not see
        // sponsored suggestions. We need to override the version-2 default-
        // branch true value by setting the pref to false.
        this.set("suggest.quicksuggest.sponsored", false);
      }
    } else {
      // The main pref is not true on the user branch, so the user either did
      // not opt in or they later disabled suggestions in the prefs UI. Set the
      // suggestion prefs to false on the user branch to override the version-2
      // default-branch true values. The data collection pref is false on the
      // default branch, but since the user was shown the modal, set it on the
      // user branch too, where it's sticky, to record the user's choice not to
      // opt in.
      this.set("suggest.quicksuggest.nonsponsored", false);
      this.set("suggest.quicksuggest.sponsored", false);
      this.set("quicksuggest.dataCollection.enabled", false);
    }
  }

  _migrateFirefoxSuggestPrefsTo_1(scenario) {
    // Copy `suggest.quicksuggest` to `suggest.quicksuggest.nonsponsored` and
    // clear the first.
    let suggestQuicksuggest = "browser.urlbar.suggest.quicksuggest";
    if (Services.prefs.prefHasUserValue(suggestQuicksuggest)) {
      this.set(
        "suggest.quicksuggest.nonsponsored",
        Services.prefs.getBoolPref(suggestQuicksuggest)
      );
      Services.prefs.clearUserPref(suggestQuicksuggest);
    }

    // In the unversioned prefs, sponsored suggestions were shown only if the
    // main suggestions pref `suggest.quicksuggest` was true, but now there are
    // two independent prefs, so disable sponsored if the main pref was false.
    if (!this.get("suggest.quicksuggest.nonsponsored")) {
      switch (scenario) {
        case "offline":
          // Set the pref on the user branch. Suggestions are enabled by default
          // for offline; we want to preserve the user's choice of opting out,
          // and we want to preserve the default-branch true value.
          this.set("suggest.quicksuggest.sponsored", false);
          break;
        case "online":
          // If the user-branch value is true, clear it so the default-branch
          // false value becomes the effective value.
          if (this.get("suggest.quicksuggest.sponsored")) {
            this.clear("suggest.quicksuggest.sponsored");
          }
          break;
      }
    }

    // The data collection pref is new in this version. Enable it iff the
    // scenario is online and the user opted in to suggestions. In offline, it
    // should always start off false.
    if (scenario == "online" && this.get("suggest.quicksuggest.nonsponsored")) {
      this.set("quicksuggest.dataCollection.enabled", true);
    }
  }

  _migrateFirefoxSuggestPrefsTo_2(scenario) {
    // In previous versions of the prefs for online, suggestions were disabled
    // by default; in version 2, they're enabled by default. For users who were
    // already in online and did not enable suggestions (because they did not
    // opt in, they did opt in but later disabled suggestions, or they were not
    // shown the modal) we don't want to suddenly enable them, so if the prefs
    // do not have user-branch values, set them to false.
    if (this.get("quicksuggest.scenario") == "online") {
      if (
        !Services.prefs.prefHasUserValue(
          "browser.urlbar.suggest.quicksuggest.nonsponsored"
        )
      ) {
        this.set("suggest.quicksuggest.nonsponsored", false);
      }
      if (
        !Services.prefs.prefHasUserValue(
          "browser.urlbar.suggest.quicksuggest.sponsored"
        )
      ) {
        this.set("suggest.quicksuggest.sponsored", false);
      }
    }
  }

  /**
   * @returns {Promise}
   *   This can be used to wait until the initial Firefox Suggest scenario has
   *   been set on startup. It's resolved when the first call to
   *   `updateFirefoxSuggestScenario()` finishes.
   */
  get firefoxSuggestScenarioStartupPromise() {
    return this._firefoxSuggestScenarioStartupPromise;
  }

  /**
   * @returns {boolean}
   *   Whether the Firefox Suggest scenario is being updated. While true,
   *   changes to related prefs should be ignored, depending on the observer.
   *   Telemetry intended to capture user changes to the prefs should not be
   *   recorded, for example.
   */
  get updatingFirefoxSuggestScenario() {
    return this._updatingFirefoxSuggestScenario;
  }

  /**
   * Adds a preference observer.  Observers are held weakly.
   *
   * @param {object} observer
   *        An object that may optionally implement one or both methods:
   *         - `onPrefChanged` invoked when one of the preferences listed here
   *           change. It will be passed the pref name.  For prefs in the
   *           `browser.urlbar.` branch, the name will be relative to the branch.
   *           For other prefs, the name will be the full name.
   *         - `onNimbusChanged` invoked when a Nimbus value changes. It will be
   *           passed the name of the changed Nimbus variable.
   */
  addObserver(observer) {
    this._observerWeakRefs.push(Cu.getWeakReference(observer));
  }

  /**
   * Removes a preference observer.
   *
   * @param {object} observer
   *   An observer previously added with `addObserver()`.
   */
  removeObserver(observer) {
    for (let i = 0; i < this._observerWeakRefs.length; i++) {
      let obs = this._observerWeakRefs[i].get();
      if (obs && obs == observer) {
        this._observerWeakRefs.splice(i, 1);
        break;
      }
    }
  }

  /**
   * Observes preference changes.
   *
   * @param {nsISupports} subject
   *   The subject of the notification.
   * @param {string} topic
   *   The topic of the notification.
   * @param {string} data
   *   The data attached to the notification.
   */
  observe(subject, topic, data) {
    let pref = data.replace(PREF_URLBAR_BRANCH, "");
    if (!PREF_URLBAR_DEFAULTS.has(pref) && !PREF_OTHER_DEFAULTS.has(pref)) {
      return;
    }
    this.#notifyObservers("onPrefChanged", pref);
  }

  /**
   * Called when a pref tracked by UrlbarPrefs changes.
   *
   * @param {string} pref
   *        The name of the pref, relative to `browser.urlbar.` if the pref is
   *        in that branch.
   */
  onPrefChanged(pref) {
    this._map.delete(pref);

    // Some prefs may influence others.
    switch (pref) {
      case "autoFill.adaptiveHistory.useCountThreshold":
        this._map.delete("autoFillAdaptiveHistoryUseCountThreshold");
        return;
      case "showSearchSuggestionsFirst":
        this.#resultGroups = null;
        return;
    }

    if (pref.startsWith("suggest.")) {
      this._map.delete("defaultBehavior");
    }

    if (this.shouldHandOffToSearchModePrefs.includes(pref)) {
      this._map.delete("shouldHandOffToSearchMode");
    }
  }

  /**
   * Called when the `NimbusFeatures.urlbar` value changes.
   */
  _onNimbusUpdate() {
    let oldNimbus = this._clearNimbusCache();
    let newNimbus = this._nimbus;

    // Callback to observers having onNimbusChanged.
    for (let name in newNimbus) {
      if (oldNimbus[name] != newNimbus[name]) {
        this.#notifyObservers("onNimbusChanged", name);
      }
    }

    // If a change occurred to the Firefox Suggest scenario variable or any
    // variables that correspond to prefs exposed in the UI, we need to update
    // the scenario.
    let variables = [
      "quickSuggestScenario",
      ...Object.keys(this.FIREFOX_SUGGEST_UI_PREFS_BY_VARIABLE),
    ];
    for (let name of variables) {
      if (oldNimbus[name] != newNimbus[name]) {
        this.updateFirefoxSuggestScenario();
        return;
      }
    }

    // If the current default-branch value of any pref is incorrect for the
    // intended scenario, we need to update the scenario.
    let scenario = this._getIntendedFirefoxSuggestScenario();
    let intendedDefaultPrefs = this.FIREFOX_SUGGEST_DEFAULT_PREFS[scenario];
    let defaults = Services.prefs.getDefaultBranch("browser.urlbar.");
    for (let [name, value] of Object.entries(intendedDefaultPrefs)) {
      // We assume all prefs are boolean right now.
      if (defaults.getBoolPref(name) != value) {
        this.updateFirefoxSuggestScenario();
        return;
      }
    }
  }

  /**
   * Clears cached Nimbus variables. The cache will be repopulated the next time
   * `_nimbus` is accessed.
   *
   * @returns {object}
   *   The value of the cache before it was cleared. It's an object that maps
   *   from variable names to values.
   */
  _clearNimbusCache() {
    let nimbus = this.__nimbus;
    if (nimbus) {
      for (let key of Object.keys(nimbus)) {
        this._map.delete(key);
      }
      this.__nimbus = null;
    }
    return nimbus || {};
  }

  get _nimbus() {
    if (!this.__nimbus) {
      this.__nimbus = lazy.NimbusFeatures.urlbar.getAllVariables({
        defaultValues: NIMBUS_DEFAULTS,
      });
    }
    return this.__nimbus;
  }

  /**
   * Returns the raw value of the given preference straight from Services.prefs.
   *
   * @param {string} pref
   *        The name of the preference to get.
   * @returns {*} The raw preference value.
   */
  _readPref(pref) {
    let { defaultValue, get } = this._getPrefDescriptor(pref);
    return get(pref, defaultValue);
  }

  /**
   * Returns a validated and/or fixed-up value of the given preference.  The
   * value may be validated for correctness, or it might be converted into a
   * different value that is easier to work with than the actual value stored in
   * the preferences branch.  Not all preferences require validation or fixup.
   *
   * The values returned from this method are the values that are made public by
   * this module.
   *
   * @param {string} pref
   *        The name of the preference to get.
   * @returns {*} The validated and/or fixed-up preference value.
   */
  _getPrefValue(pref) {
    switch (pref) {
      case "defaultBehavior": {
        let val = 0;
        for (let type of Object.keys(SUGGEST_PREF_TO_BEHAVIOR)) {
          let behavior = `BEHAVIOR_${SUGGEST_PREF_TO_BEHAVIOR[
            type
          ].toUpperCase()}`;
          val |=
            this.get("suggest." + type) && Ci.mozIPlacesAutoComplete[behavior];
        }
        return val;
      }
      case "shouldHandOffToSearchMode":
        return this.shouldHandOffToSearchModePrefs.some(
          prefName => !this.get(prefName)
        );
      case "autoFillAdaptiveHistoryUseCountThreshold":
        const nimbusValue =
          this._nimbus.autoFillAdaptiveHistoryUseCountThreshold;
        return nimbusValue === undefined
          ? this.get("autoFill.adaptiveHistory.useCountThreshold")
          : parseFloat(nimbusValue);
    }
    return this._readPref(pref);
  }

  /**
   * Returns a descriptor of the given preference.
   *
   * @param {string} pref The preference to examine.
   * @returns {object} An object describing the pref with the following shape:
   *          { defaultValue, get, set, clear }
   */
  _getPrefDescriptor(pref) {
    let branch = Services.prefs.getBranch(PREF_URLBAR_BRANCH);
    let defaultValue = PREF_URLBAR_DEFAULTS.get(pref);
    if (defaultValue === undefined) {
      branch = Services.prefs;
      defaultValue = PREF_OTHER_DEFAULTS.get(pref);
      if (defaultValue === undefined) {
        let nimbus = this._getNimbusDescriptor(pref);
        if (nimbus) {
          return nimbus;
        }
        throw new Error("Trying to access an unknown pref " + pref);
      }
    }

    let type;
    if (!Array.isArray(defaultValue)) {
      type = PREF_TYPES.get(typeof defaultValue);
    } else {
      if (defaultValue.length != 2) {
        throw new Error("Malformed pref def: " + pref);
      }
      [defaultValue, type] = defaultValue;
      type = PREF_TYPES.get(type);
    }
    if (!type) {
      throw new Error("Unknown pref type: " + pref);
    }
    return {
      defaultValue,
      get: branch[`get${type}Pref`],
      // Float prefs are stored as Char.
      set: branch[`set${type == "Float" ? "Char" : type}Pref`],
      clear: branch.clearUserPref,
    };
  }

  /**
   * Returns a descriptor for the given Nimbus property, if it exists.
   *
   * @param {string} name
   *   The name of the desired property in the object returned from
   *   NimbusFeatures.urlbar.getAllVariables().
   * @returns {object}
   *   An object describing the property's value with the following shape (same
   *   as _getPrefDescriptor()):
   *     { defaultValue, get, set, clear }
   *   If the property doesn't exist, null is returned.
   */
  _getNimbusDescriptor(name) {
    if (!this._nimbus.hasOwnProperty(name)) {
      return null;
    }
    return {
      defaultValue: this._nimbus[name],
      get: () => this._nimbus[name],
      set() {
        throw new Error(`'${name}' is a Nimbus value and cannot be set`);
      },
      clear() {
        throw new Error(`'${name}' is a Nimbus value and cannot be cleared`);
      },
    };
  }

  /**
   * Initializes the showSearchSuggestionsFirst pref based on the matchGroups
   * pref.  This function can be removed when the corresponding UI migration in
   * BrowserGlue.sys.mjs is no longer needed.
   */
  initializeShowSearchSuggestionsFirstPref() {
    let matchGroups = [];
    let pref = Services.prefs.getCharPref("browser.urlbar.matchGroups", "");
    try {
      matchGroups = pref.split(",").map(v => {
        let group = v.split(":");
        return [group[0].trim().toLowerCase(), Number(group[1])];
      });
    } catch (ex) {}
    let groupNames = matchGroups.map(group => group[0]);
    let suggestionIndex = groupNames.indexOf("suggestion");
    let generalIndex = groupNames.indexOf("general");
    let showSearchSuggestionsFirst =
      generalIndex < 0 ||
      (suggestionIndex >= 0 && suggestionIndex < generalIndex);
    let oldValue = Services.prefs.getBoolPref(
      "browser.urlbar.showSearchSuggestionsFirst"
    );
    Services.prefs.setBoolPref(
      "browser.urlbar.showSearchSuggestionsFirst",
      showSearchSuggestionsFirst
    );

    // Pref observers aren't called when a pref is set to its current value, but
    // we always want to set matchGroups to the appropriate default value via
    // onPrefChanged, so call it now if necessary.  This is really only
    // necessary for tests since the only time this function is called outside
    // of tests is by a UI migration in BrowserGlue.
    if (oldValue == showSearchSuggestionsFirst) {
      this.onPrefChanged("showSearchSuggestionsFirst");
    }
  }

  /**
   * Return whether or not persisted search terms is enabled.
   *
   * @returns {boolean} true: if enabled.
   */
  isPersistedSearchTermsEnabled() {
    return (
      this.get("showSearchTermsFeatureGate") &&
      this.get("showSearchTerms.enabled") &&
      !this.get("browser.search.widget.inNavBar")
    );
  }

  #notifyObservers(method, changed) {
    for (let i = 0; i < this._observerWeakRefs.length; ) {
      let observer = this._observerWeakRefs[i].get();
      if (!observer) {
        // The observer has been GC'ed, so remove it from our list.
        this._observerWeakRefs.splice(i, 1);
        continue;
      }
      if (method in observer) {
        try {
          observer[method](changed);
        } catch (ex) {
          console.error(ex);
        }
      }
      ++i;
    }
  }

  #resultGroups = null;
}

export var UrlbarPrefs = new Preferences();
