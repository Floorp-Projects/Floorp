/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports the UrlbarPrefs singleton, which manages preferences for
 * the urlbar. It also provides access to urlbar Nimbus features as if they are
 * preferences.
 */

var EXPORTED_SYMBOLS = ["UrlbarPrefs", "UrlbarPrefsObserver"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

const PREF_URLBAR_BRANCH = "browser.urlbar.";

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

  // For search suggestion results, we truncate the user's search string to this
  // number of characters before fetching results.
  ["maxCharsForSearchSuggestions", 20],

  // The maximum number of form history results to include.
  ["maxHistoricalSearchSuggestions", 0],

  // The maximum number of results in the urlbar popup.
  ["maxRichResults", 10],

  // Whether addresses and search results typed into the address bar
  // should be opened in new tabs by default.
  ["openintab", false],

  // When true, URLs in the user's history that look like search result pages
  // are styled to look like search engine results instead of the usual history
  // results.
  ["restyleSearches", false],

  // Controls the composition of results.  The default value is computed by
  // calling:
  //   makeResultBuckets({
  //     showSearchSuggestionsFirst: UrlbarPrefs.get(
  //       "showSearchSuggestionsFirst"
  //     ),
  //   });
  // The value of this pref is a JSON string of the root bucket.  See below.
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

  // Whether results will include the user's history.
  ["suggest.history", true],

  // Whether results will include switch-to-tab results.
  ["suggest.openpage", true],

  // Whether results will include search suggestions.
  ["suggest.searches", false],

  // Whether results will include search engines (e.g. tab-to-search).
  ["suggest.engines", true],

  // Whether results will include top sites and the view will open on focus.
  ["suggest.topsites", true],

  // Whether results will include a calculator.
  ["suggest.calculator", false],

  // Whether results will include QuickSuggest suggestions.
  ["suggest.quicksuggest", true],

  // Whether the user has seen the onboarding dialog.
  ["quicksuggest.showedOnboardingDialog", false],

  // Count the restarts before showing the onboarding dialog.
  ["quicksuggest.seenRestarts", 0],

  // Whether to show QuickSuggest related logs.
  ["quicksuggest.log", false],

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
 * Builds the standard result buckets and returns the root bucket.  Result
 * buckets determine the composition of results in the muxer, i.e., how they're
 * grouped and sorted.  Each bucket is an object that looks like this:
 *
 * {
 *   {UrlbarUtils.RESULT_GROUP} [group]
 *     This is defined only on buckets without children, and it determines the
 *     result group that the bucket will contain.
 *   {number} [maxResultCount]
 *     An optional maximum number of results the bucket can contain.  If it's
 *     not defined and the parent bucket does not define `flexChildren: true`,
 *     then the max is the parent's max.  If the parent bucket defines
 *     `flexChildren: true`, then `maxResultCount` is ignored.
 *   {boolean} [flexChildren]
 *     If true, then child buckets are "flexed", similar to flex in HTML.  Each
 *     child bucket should define the `flex` property (or, if they don't, `flex`
 *     is assumed to be zero).  `flex` is a number that defines the ratio of a
 *     child's result count to the total result count of all children.  More
 *     specifically, `flex: X` on a child means that the initial maximum result
 *     count of the child is `parentMaxResultCount * (X / N)`, where `N` is the
 *     sum of the `flex` values of all children.  If there are any child buckets
 *     that cannot be completely filled, then the muxer will attempt to overfill
 *     the children that were completely filled, while still respecting their
 *     relative `flex` values.
 *   {number} [flex]
 *     The flex value of the bucket.  This should be defined only on buckets
 *     where the parent defines `flexChildren: true`.  See `flexChildren` for a
 *     discussion of flex.
 *   {array} [children]
 *     An array of child bucket objects.
 * }
 *
 * @param {boolean} showSearchSuggestionsFirst
 *   If true, the suggestions bucket will come before the general bucket.
 * @returns {object}
 *   The root bucket.
 */
function makeResultBuckets({ showSearchSuggestionsFirst }) {
  let rootBucket = {
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

  // Prepare the parent bucket for suggestions and general.
  let mainBucket = {
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
    mainBucket.children.reverse();
  }
  mainBucket.children[0].flex = 2;
  mainBucket.children[1].flex = 1;
  rootBucket.children.push(mainBucket);

  return rootBucket;
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
   * Builds the standard result buckets.  See makeResultBuckets.
   *
   * @param {object} options
   *   See makeResultBuckets.
   * @returns {object}
   *   The root bucket.
   */
  makeResultBuckets(options) {
    return makeResultBuckets(options);
  }

  /**
   * Sets the value of the resultGroups pref to the current default buckets.
   * This should be called from BrowserGlue._migrateUI when the default buckets
   * are modified.
   */
  migrateResultBuckets() {
    this.set(
      "resultGroups",
      JSON.stringify(
        makeResultBuckets({
          showSearchSuggestionsFirst: this.get("showSearchSuggestionsFirst"),
        })
      )
    );
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
            makeResultBuckets({ showSearchSuggestionsFirst: this.get(pref) })
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
        return makeResultBuckets({
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
   * Initializes the showSearchSuggestionsFirst pref based on the matchBuckets
   * pref.  This function can be removed when the corresponding UI migration in
   * BrowserGlue.jsm is no longer needed.
   */
  initializeShowSearchSuggestionsFirstPref() {
    let matchBuckets = [];
    let pref = Services.prefs.getCharPref("browser.urlbar.matchBuckets", "");
    try {
      matchBuckets = pref.split(",").map(v => {
        let bucket = v.split(":");
        return [bucket[0].trim().toLowerCase(), Number(bucket[1])];
      });
    } catch (ex) {}
    let bucketNames = matchBuckets.map(bucket => bucket[0]);
    let suggestionIndex = bucketNames.indexOf("suggestion");
    let generalIndex = bucketNames.indexOf("general");
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
    // we always want to set matchBuckets to the appropriate default value via
    // onPrefChanged, so call it now if necessary.  This is really only
    // necessary for tests since the only time this function is called outside
    // of tests is by a UI migration in BrowserGlue.
    if (oldValue == showSearchSuggestionsFirst) {
      this.onPrefChanged("showSearchSuggestionsFirst");
    }
  }
}

var UrlbarPrefs = new Preferences();
