/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports the UrlbarPrefs singleton, which manages preferences for
 * the urlbar. It also provides access to urlbar Nimbus variables as if they are
 * preferences, but only for variables with fallback prefs.
 */

var EXPORTED_SYMBOLS = ["UrlbarPrefs", "UrlbarPrefsObserver"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
  Region: "resource://gre/modules/Region.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

const PREF_URLBAR_BRANCH = "browser.urlbar.";

const FIREFOX_SUGGEST_UPDATE_TOPIC = "firefox-suggest-update";

// Prefs are defined as [pref name, default value] or [pref name, [default
// value, type]].  In the former case, the getter method name is inferred from
// the typeof the default value.
//
// NOTE: Don't name prefs (relative to the `browser.urlbar` branch) the same as
// Nimbus urlbar features. Doing so would cause a name collision because pref
// names and Nimbus feature names are both kept as keys in UrlbarPref's map. For
// a list of Nimbus features, see: toolkit/components/nimbus/FeatureManifest.js
const PREF_URLBAR_DEFAULTS = new Map([
  // Whether we announce to screen readers when tab-to-search results are
  // inserted.
  ["accessibility.tabToSearch.announceResults", true],

  // "Autofill" is the name of the feature that automatically completes domains
  // and URLs that the user has visited as the user is typing them in the urlbar
  // textbox.  If false, autofill will be disabled.
  ["autoFill", true],

  // If true, the domains of the user's installed search engines will be
  // autofilled even if the user hasn't actually visited them.
  ["autoFill.searchEngines", false],

  // Affects the frecency threshold of the autofill algorithm.  The threshold is
  // the mean of all origin frecencies plus one standard deviation multiplied by
  // this value.  See UrlbarProviderPlaces.
  ["autoFill.stddevMultiplier", [0.0, "float"]],

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
  ["dnsResolveSingleWordsAfterSearch", 1],

  // Whether telemetry events should be recorded.
  ["eventTelemetry.enabled", false],

  // Whether we expand the font size when when the urlbar is
  // focused.
  ["experimental.expandTextOnFocus", false],

  // Whether the heuristic result is hidden.
  ["experimental.hideHeuristic", false],

  // Whether the urlbar displays a permanent search button.
  ["experimental.searchButton", false],

  // When we send events to extensions, we wait this amount of time in
  // milliseconds for them to respond before timing out.
  ["extension.timeout", 400],

  // When true, `javascript:` URLs are not included in search results.
  ["filter.javascript", true],

  // Applies URL highlighting and other styling to the text in the urlbar input.
  ["formatting.enabled", true],

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

  // Whether Merino is enabled as a quick suggest source.
  ["merino.enabled", false],

  // The Merino endpoint URL, not including parameters.
  ["merino.endpointURL", "https://merino.services.mozilla.com/api/v1/suggest"],

  // Timeout for Merino fetches (ms).
  ["merino.timeoutMs", 200],

  // Whether addresses and search results typed into the address bar
  // should be opened in new tabs by default.
  ["openintab", false],

  // When true, URLs in the user's history that look like search result pages
  // are styled to look like search engine results instead of the usual history
  // results.
  ["restyleSearches", false],

  // Controls the composition of results.  The default value is computed by
  // calling:
  //   makeResultGroups({
  //     showSearchSuggestionsFirst: UrlbarPrefs.get(
  //       "showSearchSuggestionsFirst"
  //     ),
  //   });
  // The value of this pref is a JSON string of the root group.  See below.
  ["resultGroups", ""],

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

  // Whether to show search suggestions before general results.
  ["showSearchSuggestionsFirst", true],

  // Whether speculative connections should be enabled.
  ["speculativeConnect.enabled", true],

  // Whether results will include the user's bookmarks.
  ["suggest.bookmark", true],

  // Whether results will include a calculator.
  ["suggest.calculator", false],

  // Whether results will include search engines (e.g. tab-to-search).
  ["suggest.engines", true],

  // Whether results will include the user's history.
  ["suggest.history", true],

  // Whether results will include switch-to-tab results.
  ["suggest.openpage", true],

  // Whether results will include non-sponsored quick suggest suggestions.
  ["suggest.quicksuggest.nonsponsored", false],

  // Whether results will include sponsored quick suggest suggestions.
  ["suggest.quicksuggest.sponsored", false],

  // Whether results will include search suggestions.
  ["suggest.searches", false],

  // Whether results will include top sites and the view will open on focus.
  ["suggest.topsites", true],

  // Global toggle for whether the quick suggest feature is enabled, i.e.,
  // sponsored and recommended results related to the user's search string.
  ["quicksuggest.enabled", false],

  // Whether to show QuickSuggest related logs.
  ["quicksuggest.log", false],

  // The user's response to the Firefox Suggest online opt-in dialog.
  ["quicksuggest.onboardingDialogChoice", ""],

  // If the user has gone through a quick suggest prefs migration, then this
  // pref will have a user-branch value that records the latest prefs version.
  //
  // Versions:
  //
  // Unversioned (0): When `suggest.quicksuggest` is false, all quick suggest
  // results are disabled and `suggest.quicksuggest.sponsored` is ignored. To
  // show sponsored suggestions, both prefs must be true.
  // `quicksuggest.dataCollection.enabled` does not exist.
  //
  // 1: `suggest.quicksuggest` is removed, `suggest.quicksuggest.nonsponsored`
  // is introduced. `suggest.quicksuggest.nonsponsored` and
  // `suggest.quicksuggest.sponsored` are independent:
  // `suggest.quicksuggest.nonsponsored` controls non-sponsored results and
  // `suggest.quicksuggest.sponsored` controls sponsored results.
  // `quicksuggest.dataCollection.enabled` is introduced.
  ["quicksuggest.migrationVersion", 0],

  // Whether Remote Settings is enabled as a quick suggest source.
  ["quicksuggest.remoteSettings.enabled", true],

  // The Firefox Suggest scenario in which the user is enrolled. This is set
  // when the scenario is updated (see `updateFirefoxSuggestScenario`) and is
  // not a pref the user should set. Once initialized, its value is one of:
  // "history", "offline", "online"
  ["quicksuggest.scenario", ""],

  // Whether the user has opted in to data collection for quick suggest.
  ["quicksuggest.dataCollection.enabled", false],

  // Whether to show the quick suggest onboarding dialog.
  ["quicksuggest.shouldShowOnboardingDialog", true],

  // Whether the user has seen the onboarding dialog.
  ["quicksuggest.showedOnboardingDialog", false],

  // Count the restarts before showing the onboarding dialog.
  ["quicksuggest.seenRestarts", 0],

  // When using switch to tabs, if set to true this will move the tab into the
  // active window.
  ["switchTabs.adoptIntoActiveWindow", false],

  // The number of remaining times the user can interact with tab-to-search
  // onboarding results before we stop showing them.
  ["tabToSearch.onboard.interactionsLeft", 3],

  // The number of times the user has been shown the onboarding search tip.
  ["tipShownCount.searchTip_onboard", 0],

  // The number of times the user has been shown the redirect search tip.
  ["tipShownCount.searchTip_redirect", 0],

  // Remove redundant portions from URLs.
  ["trimURLs", true],

  // If true, top sites may include sponsored ones.
  ["sponsoredTopSites", false],

  // Whether unit conversion is enabled.
  ["unitConversion.enabled", false],

  // The index where we show unit conversion results.
  ["unitConversion.suggestedIndex", 1],

  // Results will include a built-in set of popular domains when this is true.
  ["usepreloadedtopurls.enabled", false],

  // After this many days from the profile creation date, the built-in set of
  // popular domains will no longer be included in the results.
  ["usepreloadedtopurls.expire_days", 14],

  // Controls the empty search behavior in Search Mode:
  //  0 - Show nothing
  //  1 - Show search history
  //  2 - Show search and browsing history
  ["update2.emptySearchBehavior", 0],
]);
const PREF_OTHER_DEFAULTS = new Map([
  ["browser.fixup.dns_first_for_single_words", false],
  ["browser.search.suggest.enabled", true],
  ["browser.search.suggest.enabled.private", false],
  ["keyword.enabled", true],
  ["ui.popup.disable_autohide", false],
]);

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
          { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_TEST },
          { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_EXTENSION },
          { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_SEARCH_TIP },
          { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_OMNIBOX },
          { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_ENGINE_ALIAS },
          { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_BOOKMARK_KEYWORD },
          { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_AUTOFILL },
          { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_PRELOADED },
          { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_TOKEN_ALIAS_ENGINE },
          { group: UrlbarUtils.RESULT_GROUP.HEURISTIC_FALLBACK },
        ],
      },
      // extensions using the omnibox API
      {
        group: UrlbarUtils.RESULT_GROUP.OMNIBOX,
        availableSpan: UrlbarUtils.MAX_OMNIBOX_RESULT_COUNT - 1,
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
                group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
              },
              {
                flex: 4,
                group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
              },
            ],
          },
          {
            group: UrlbarUtils.RESULT_GROUP.TAIL_SUGGESTION,
          },
        ],
      },
      // general
      {
        group: UrlbarUtils.RESULT_GROUP.GENERAL_PARENT,
        children: [
          {
            availableSpan: 3,
            group: UrlbarUtils.RESULT_GROUP.INPUT_HISTORY,
          },
          {
            flexChildren: true,
            children: [
              {
                flex: 1,
                group: UrlbarUtils.RESULT_GROUP.REMOTE_TAB,
              },
              {
                flex: 2,
                group: UrlbarUtils.RESULT_GROUP.GENERAL,
              },
              {
                // We show relatively many about-page results because they're
                // only added for queries starting with "about:".
                flex: 2,
                group: UrlbarUtils.RESULT_GROUP.ABOUT_PAGES,
              },
              {
                flex: 1,
                group: UrlbarUtils.RESULT_GROUP.PRELOADED,
              },
            ],
          },
          {
            group: UrlbarUtils.RESULT_GROUP.INPUT_HISTORY,
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

    // This is set to true when we update the Firefox Suggest scenario to
    // prevent re-entry due to pref observers.
    this._updatingFirefoxSuggestScenario = false;

    NimbusFeatures.urlbar.onUpdate(() => this._onNimbusUpdate());
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

  /**
   * Sets the value of the resultGroups pref to the current default groups.
   * This should be called from BrowserGlue._migrateUI when the default groups
   * are modified.
   */
  migrateResultGroups() {
    this.set(
      "resultGroups",
      JSON.stringify(
        makeResultGroups({
          showSearchSuggestionsFirst: this.get("showSearchSuggestionsFirst"),
        })
      )
    );
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
   *   This is the scenario for the "online" rollout. The onboarding dialog will
   *   be shown and the user must opt in to enable Firefox Suggest suggestions
   *   and data collection. If the user does not opt in via the dialog, they can
   *   opt in later in about:preferences.
   *
   * @param {boolean} isStartup
   *   Pass true when calling at startup. Appropriate pref migrations are
   *   applied in that case.
   * @param {string} [scenarioOverride]
   *   This is intended for tests only. Pass to force a scenario.
   */
  async updateFirefoxSuggestScenario(isStartup, scenarioOverride = undefined) {
    // Make sure we don't re-enter this method while updating prefs. Updates to
    // prefs that are fallbacks for Nimbus variables trigger the pref observer
    // in Nimbus, which triggers our Nimbus `onUpdate` callback, which calls
    // this method again.
    if (this._updatingFirefoxSuggestScenario) {
      return;
    }

    try {
      this._updatingFirefoxSuggestScenario = true;

      // This is called early in startup by BrowserGlue, so make sure the user's
      // region and our Nimbus variables are initialized since the scenario may
      // depend on them. Also note that pref migrations may depend on the
      // scenario, and since each migration is performed only once, at startup,
      // prefs can end up wrong if their migrations use the wrong scenario.
      await Region.init();
      await NimbusFeatures.urlbar.ready();

      this._updateFirefoxSuggestScenarioHelper(isStartup, scenarioOverride);
    } finally {
      this._updatingFirefoxSuggestScenario = false;
    }
  }

  _updateFirefoxSuggestScenarioHelper(isStartup, scenarioOverride) {
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

    let nonSponsoredInitiallyEnabled = this.get(
      "suggest.quicksuggest.nonsponsored"
    );
    let sponsoredInitiallyEnabled = this.get("suggest.quicksuggest.sponsored");

    // 1. Pick a scenario
    let scenario = scenarioOverride || this._nimbus.quickSuggestScenario;
    if (!scenario) {
      if (
        Region.home == "US" &&
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
      Cu.reportError(`Unrecognized Firefox Suggest scenario "${scenario}"`);
    }

    // 2. Set default-branch values for the scenario
    let prefs = this.FIREFOX_SUGGEST_DEFAULT_PREFS[scenario];

    // 3. Set default-branch values for prefs that are both exposed in the UI
    // and configurable via Nimbus
    let uiPrefNamesByVariable = {
      quickSuggestNonSponsoredEnabled: "suggest.quicksuggest.nonsponsored",
      quickSuggestSponsoredEnabled: "suggest.quicksuggest.sponsored",
      quickSuggestDataCollectionEnabled: "quicksuggest.dataCollection.enabled",
    };
    for (let [variable, prefName] of Object.entries(uiPrefNamesByVariable)) {
      if (this._nimbus.hasOwnProperty(variable)) {
        prefs[prefName] = this._nimbus[variable];
      }
    }

    let defaults = Services.prefs.getDefaultBranch("browser.urlbar.");
    for (let [name, value] of Object.entries(prefs)) {
      // We assume all prefs are boolean right now.
      defaults.setBoolPref(name, value);
    }

    // The online scenario disables suggestions by default so that we can prompt
    // the user to opt in to suggestions and data collection all at once.
    // However, if the user is coming to online from a scenario where
    // suggestions were enabled by default *and* the user has already made the
    // choice to opt in to data (`quicksuggest.dataCollection.enabled` is true
    // on the user branch), then go ahead and opt them in to suggestions too.
    //
    // The `prefHasUserValue` check isn't necessary because if the scenario is
    // online, `quicksuggest.dataCollection.enabled` is false by default (it's
    // also false in firefox.js), so if `quicksuggest.dataCollection.enabled` is
    // true then it must be true on the user branch. We check it anyway to be
    // defensive and to guard against errors if this code or pref defaults are
    // changed in the future.
    if (
      scenario == "online" &&
      this.get("quicksuggest.dataCollection.enabled") &&
      Services.prefs.prefHasUserValue(
        "browser.urlbar.quicksuggest.dataCollection.enabled"
      )
    ) {
      if (nonSponsoredInitiallyEnabled) {
        this.set("suggest.quicksuggest.nonsponsored", true);
      }
      if (sponsoredInitiallyEnabled) {
        this.set("suggest.quicksuggest.sponsored", true);
      }
    }

    // 4. Migrate prefs across app versions
    if (isStartup) {
      this._ensureFirefoxSuggestPrefsMigrated(scenario);
    }

    // Set the scenario pref only after migrating so that migrations can tell
    // what the last-seen scenario was. Set it on the user branch so that its
    // value persists across app restarts.
    this.set("quicksuggest.scenario", scenario);

    // Update the pref cache in TelemetryEnvironment. This is only necessary
    // when we're initializing the scenario on startup, but the scenario will
    // rarely if ever change after that, so there's no harm in always doing it.
    // See bug 1731373.
    //
    // IMPORTANT: Send the notification only after setting the prefs above.
    // TelemetryEnvironment updates its cache by fetching the prefs from the
    // pref service, so the new values need to be set beforehand. See also the
    // comments in D126017.
    Services.obs.notifyObservers(null, FIREFOX_SUGGEST_UPDATE_TOPIC);
  }

  /**
   * Default prefs relative to `browser.urlbar` per Firefox Suggest
   * scenario. This returns a new object on every access, so it's OK to modify
   * it.
   */
  get FIREFOX_SUGGEST_DEFAULT_PREFS() {
    // Important notes when modifying this:
    //
    // Callers currently assume a new object is returned per access.
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
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
      },
    };
  }

  /**
   * Migrates the unversioned set of Firefox Suggest prefs to version 1.
   *
   * @param {string} scenario
   *   The current Firefox Suggest scenario.
   */
  _ensureFirefoxSuggestPrefsMigrated(scenario) {
    if (this.get("quicksuggest.migrationVersion")) {
      // Already migrated to version 1.
      return;
    }

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

    this.set("quicksuggest.migrationVersion", 1);
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
   *        An object that must have a method named `onPrefChanged`, which will
   *        be called when a urlbar preference changes.  It will be passed the
   *        pref name.  For prefs in the `browser.urlbar.` branch, the name will
   *        be relative to the branch.  For other prefs, the name will be the
   *        full name.
   */
  addObserver(observer) {
    this._observerWeakRefs.push(Cu.getWeakReference(observer));
  }

  /**
   * Observes preference changes.
   *
   * @param {nsISupports} subject
   * @param {string} topic
   * @param {string} data
   */
  observe(subject, topic, data) {
    let pref = data.replace(PREF_URLBAR_BRANCH, "");
    if (!PREF_URLBAR_DEFAULTS.has(pref) && !PREF_OTHER_DEFAULTS.has(pref)) {
      return;
    }
    for (let i = 0; i < this._observerWeakRefs.length; ) {
      let observer = this._observerWeakRefs[i].get();
      if (!observer) {
        // The observer has been GC'ed, so remove it from our list.
        this._observerWeakRefs.splice(i, 1);
      } else {
        observer.onPrefChanged(pref);
        ++i;
      }
    }
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
      case "showSearchSuggestionsFirst":
        this.set(
          "resultGroups",
          JSON.stringify(
            makeResultGroups({ showSearchSuggestionsFirst: this.get(pref) })
          )
        );
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
    for (let key of Object.keys(this._nimbus)) {
      this._map.delete(key);
    }
    this.__nimbus = null;

    this.updateFirefoxSuggestScenario(false);
  }

  get _nimbus() {
    if (!this.__nimbus) {
      this.__nimbus = NimbusFeatures.urlbar.getAllVariables();
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
      case "resultGroups":
        try {
          return JSON.parse(this._readPref(pref));
        } catch (ex) {}
        return makeResultGroups({
          showSearchSuggestionsFirst: this.get("showSearchSuggestionsFirst"),
        });
      case "shouldHandOffToSearchMode":
        return this.shouldHandOffToSearchModePrefs.some(
          prefName => !this.get(prefName)
        );
    }
    return this._readPref(pref);
  }

  /**
   * Returns a descriptor of the given preference.
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
   * BrowserGlue.jsm is no longer needed.
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
}

var UrlbarPrefs = new Preferences();
