/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports the UrlbarPrefs singleton, which manages
 * preferences for the urlbar.
 */

var EXPORTED_SYMBOLS = ["UrlbarPrefs"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

const PREF_URLBAR_BRANCH = "browser.urlbar.";

// Prefs are defined as [pref name, default value] or [pref name, [default
// value, type]].  In the former case, the getter method name is inferred from
// the typeof the default value.
const PREF_URLBAR_DEFAULTS = new Map([
  // "Autofill" is the name of the feature that automatically completes domains
  // and URLs that the user has visited as the user is typing them in the urlbar
  // textbox.  If false, autofill will be disabled.
  ["autoFill", true],

  // If true, the domains of the user's installed search engines will be
  // autofilled even if the user hasn't actually visited them.
  ["autoFill.searchEngines", false],

  // Affects the frecency threshold of the autofill algorithm.  The threshold is
  // the mean of all origin frecencies plus one standard deviation multiplied by
  // this value.  See UnifiedComplete.
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

  // Whether the urlbar displays a permanent search button.
  ["experimental.searchButton", false],

  // When true, `javascript:` URLs are not included in search results.
  ["filter.javascript", true],

  // Applies URL highlighting and other styling to the text in the urlbar input.
  ["formatting.enabled", true],

  // Controls the composition of search results.
  ["matchBuckets", "suggestion:4,general:Infinity"],

  // If the heuristic result is a search engine result, we use this instead of
  // matchBuckets.
  ["matchBucketsSearch", ""],

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

  // If true, we show tail suggestions when available.
  ["richSuggestions.tail", true],

  // Hidden pref. Disables checks that prevent search tips being shown, thus
  // showing them every time the newtab page or the default search engine
  // homepage is opened.
  ["searchTips.test.ignoreShowLimits", false],

  // Whether speculative connections should be enabled.
  ["speculativeConnect.enabled", true],

  // Results will include the user's bookmarks when this is true.
  ["suggest.bookmark", true],

  // Results will include the user's history when this is true.
  ["suggest.history", true],

  // Results will include switch-to-tab results when this is true.
  ["suggest.openpage", true],

  // Results will include search suggestions when this is true.
  ["suggest.searches", false],

  // Results will include Top Sites and the view will open on focus when this
  // is true.
  ["suggest.topsites", true],

  // When using switch to tabs, if set to true this will move the tab into the
  // active window.
  ["switchTabs.adoptIntoActiveWindow", false],

  // The number of times the user has been shown the onboarding search tip.
  ["tipShownCount.searchTip_onboard", 0],

  // The number of times the user has been shown the redirect search tip.
  ["tipShownCount.searchTip_redirect", 0],

  // Remove redundant portions from URLs.
  ["trimURLs", true],

  // Results will include a built-in set of popular domains when this is true.
  ["usepreloadedtopurls.enabled", false],

  // After this many days from the profile creation date, the built-in set of
  // popular domains will no longer be included in the results.
  ["usepreloadedtopurls.expire_days", 14],

  // Whether aliases are styled as a "chiclet" separated from the Urlbar.
  // Also controls the other urlbar.update2 prefs.
  ["update2", false],

  // Whether the urlbar displays one-offs to filter searches to history,
  // bookmarks, or tabs.
  ["update2.localOneOffs", false],

  // Whether the urlbar one-offs act as search filters instead of executing a
  // search immediately.
  ["update2.oneOffsRefresh", false],

  // Whether we display a tab-to-complete result when the user types an engine
  // name.
  ["update2.tabToComplete", false],
]);
const PREF_OTHER_DEFAULTS = new Map([
  ["keyword.enabled", true],
  ["browser.search.suggest.enabled", true],
  ["browser.search.suggest.enabled.private", false],
  ["ui.popup.disable_autohide", false],
  ["browser.fixup.dns_first_for_single_words", false],
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

// Buckets for result insertion.
// Every time a new result is returned, we go through each bucket in array order,
// and look for the first one having available space for the given result type.
// Each bucket is an array containing the following indices:
//   0: The result type of the acceptable entries.
//   1: available number of slots in this bucket.
// There are different matchBuckets definition for different contexts, currently
// a general one (matchBuckets) and a search one (matchBucketsSearch).
//
// First buckets. Anything with an Infinity frecency ends up here.
const DEFAULT_BUCKETS_BEFORE = [
  [UrlbarUtils.RESULT_GROUP.HEURISTIC, 1],
  [
    UrlbarUtils.RESULT_GROUP.EXTENSION,
    UrlbarUtils.MAXIMUM_ALLOWED_EXTENSION_MATCHES - 1,
  ],
];
// => USER DEFINED BUCKETS WILL BE INSERTED HERE <=
//
// Catch-all buckets. Anything remaining ends up here.
const DEFAULT_BUCKETS_AFTER = [
  [UrlbarUtils.RESULT_GROUP.SUGGESTION, Infinity],
  [UrlbarUtils.RESULT_GROUP.GENERAL, Infinity],
];

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
    if (!this._map.has(pref)) {
      this._map.set(pref, this._getPrefValue(pref));
    }
    return this._map.get(pref);
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
    let { defaultValue, setter } = this._getPrefDescriptor(pref);
    if (typeof value != typeof defaultValue) {
      throw new Error(`Invalid value type ${typeof value} for pref ${pref}`);
    }
    setter(pref, value);
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
    this._map.delete(pref);
    // Some prefs may influence others.
    if (pref == "matchBuckets") {
      this._map.delete("matchBucketsSearch");
    }
    if (pref.startsWith("suggest.")) {
      this._map.delete("defaultBehavior");
    }
  }

  /**
   * Returns the raw value of the given preference straight from Services.prefs.
   *
   * @param {string} pref
   *        The name of the preference to get.
   * @returns {*} The raw preference value.
   */
  _readPref(pref) {
    let { defaultValue, getter } = this._getPrefDescriptor(pref);
    return getter(pref, defaultValue);
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
      case "matchBuckets": {
        // Convert from pref char format to an array and add the default
        // buckets.
        let val = this._readPref(pref);
        try {
          val = PlacesUtils.convertMatchBucketsStringToArray(val);
        } catch (ex) {
          val = PlacesUtils.convertMatchBucketsStringToArray(
            PREF_URLBAR_DEFAULTS.get(pref)
          );
        }
        return [...DEFAULT_BUCKETS_BEFORE, ...val, ...DEFAULT_BUCKETS_AFTER];
      }
      case "matchBucketsSearch": {
        // Convert from pref char format to an array and add the default
        // buckets.
        let val = this._readPref(pref);
        if (val) {
          // Convert from pref char format to an array and add the default
          // buckets.
          try {
            val = PlacesUtils.convertMatchBucketsStringToArray(val);
            return [
              ...DEFAULT_BUCKETS_BEFORE,
              ...val,
              ...DEFAULT_BUCKETS_AFTER,
            ];
          } catch (ex) {
            /* invalid format, will just return matchBuckets */
          }
        }
        return this.get("matchBuckets");
      }
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
    }
    return this._readPref(pref);
  }

  /**
   * Returns a descriptor of the given preference.
   * @param {string} pref The preference to examine.
   * @returns {object} An object describing the pref with the following shape:
   *          { defaultValue, getter, setter }
   */
  _getPrefDescriptor(pref) {
    let branch = Services.prefs.getBranch(PREF_URLBAR_BRANCH);
    let defaultValue = PREF_URLBAR_DEFAULTS.get(pref);
    if (defaultValue === undefined) {
      branch = Services.prefs;
      defaultValue = PREF_OTHER_DEFAULTS.get(pref);
    }
    if (defaultValue === undefined) {
      throw new Error("Trying to access an unknown pref " + pref);
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
      getter: branch[`get${type}Pref`],
      // Float prefs are stored as Char.
      setter: branch[`set${type == "Float" ? "Char" : type}Pref`],
    };
  }
}

var UrlbarPrefs = new Preferences();
