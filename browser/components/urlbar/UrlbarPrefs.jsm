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
// value, nsIPrefBranch getter method name]].  In the former case, the getter
// method name is inferred from the typeof the default value.
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
  ["autoFill.stddevMultiplier", [0.0, "getFloatPref"]],

  // If true, this optimizes for replacing the full URL rather than editing
  // part of it. This also copies the urlbar value to the selection clipboard
  // on systems that support it.
  ["clickSelectsAll", false],

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

  // If true, this optimizes for replacing the full URL rather than selecting a
  // portion of it. This also copies the urlbar value to the selection
  // clipboard on systems that support it.
  ["doubleClickSelectsAll", false],

  // Whether telemetry events should be recorded.
  ["eventTelemetry.enabled", false],

  // When true, `javascript:` URLs are not included in search results.
  ["filter.javascript", true],

  // Applies URL highlighting and other styling to the text in the urlbar input.
  ["formatting.enabled", false],

  // Allows results from one search to be reused in the next search.  One of the
  // INSERTMETHOD values.
  ["insertMethod", UrlbarUtils.INSERTMETHOD.MERGE_RELATED],

  // Controls the composition of search results.
  ["matchBuckets", "suggestion:4,general:Infinity"],

  // If the heuristic result is a search engine result, we use this instead of
  // matchBuckets.
  ["matchBucketsSearch", ""],

  // For search suggestion results, we truncate the user's search string to this
  // number of characters before fetching results.
  ["maxCharsForSearchSuggestions", 20],

  // May be removed in the future.  Usually (when this pref is at its default of
  // zero), search engine results do not include results from the user's local
  // browser history.  This value can be set to include such results.
  ["maxHistoricalSearchSuggestions", 0],

  // The maximum number of results in the urlbar popup.
  ["maxRichResults", 10],

  // One-off search buttons enabled status.
  ["oneOffSearches", false],

  // Whether addresses and search results typed into the address bar
  // should be opened in new tabs by default.
  ["openintab", false],

  // Whether to open the urlbar view when the input field is focused by the user.
  ["openViewOnFocus", false],

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

  // When using switch to tabs, if set to true this will move the tab into the
  // active window.
  ["switchTabs.adoptIntoActiveWindow", false],

  // Remove redundant portions from URLs.
  ["trimURLs", true],

  // Results will include a built-in set of popular domains when this is true.
  ["usepreloadedtopurls.enabled", true],

  // After this many days from the profile creation date, the built-in set of
  // popular domains will no longer be included in the results.
  ["usepreloadedtopurls.expire_days", 14],

  // When true, URLs in the user's history that look like search result pages
  // are styled to look like search engine results instead of the usual history
  // results.
  ["restyleSearches", false],
]);
const PREF_OTHER_DEFAULTS = new Map([
  ["keyword.enabled", true],
  ["browser.search.suggest.enabled", true],
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
  ["string", "Char"],
  ["number", "Int"],
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
      Ci.nsIObserver,
      Ci.nsISupportsWeakReference,
    ]);
    Services.prefs.addObserver(PREF_URLBAR_BRANCH, this, true);
    for (let pref of PREF_OTHER_DEFAULTS.keys()) {
      Services.prefs.addObserver(pref, this, true);
    }
  }

  /**
   * Returns the value for the preference with the given name.
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
      this._map.delete("emptySearchDefaultBehavior");
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
    let prefs = Services.prefs.getBranch(PREF_URLBAR_BRANCH);
    let def = PREF_URLBAR_DEFAULTS.get(pref);
    if (def === undefined) {
      prefs = Services.prefs;
      def = PREF_OTHER_DEFAULTS.get(pref);
    }
    if (def === undefined) {
      throw new Error("Trying to access an unknown pref " + pref);
    }
    let getterName;
    if (!Array.isArray(def)) {
      getterName = `get${PREF_TYPES.get(typeof def)}Pref`;
    } else {
      if (def.length != 2) {
        throw new Error("Malformed pref def: " + pref);
      }
      [def, getterName] = def;
    }
    return prefs[getterName](pref, def);
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
      case "emptySearchDefaultBehavior": {
        // Further restrictions to apply for "empty searches" (searching for
        // "").  The empty behavior is typed history, if history is enabled.
        // Otherwise, it is bookmarks, if they are enabled. If both history and
        // bookmarks are disabled, it defaults to open pages.
        let val = Ci.mozIPlacesAutoComplete.BEHAVIOR_RESTRICT;
        if (this.get("suggest.history")) {
          val |= Ci.mozIPlacesAutoComplete.BEHAVIOR_HISTORY;
        } else if (this.get("suggest.bookmark")) {
          val |= Ci.mozIPlacesAutoComplete.BEHAVIOR_BOOKMARK;
        } else {
          val |= Ci.mozIPlacesAutoComplete.BEHAVIOR_OPENPAGE;
        }
        return val;
      }
    }
    return this._readPref(pref);
  }
}

var UrlbarPrefs = new Preferences();
