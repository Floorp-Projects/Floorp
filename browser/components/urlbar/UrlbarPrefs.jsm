/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports the UrlbarPrefs singleton, which manages
 * preferences for the urlbar.
 */

var EXPORTED_SYMBOLS = ["UrlbarPrefs"];

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

const PREF_URLBAR_BRANCH = "browser.urlbar.";

// Prefs are defined as [pref name, default value] or [pref name, [default
// value, nsIPrefBranch getter method name]].  In the former case, the getter
// method name is inferred from the typeof the default value.
const PREF_URLBAR_DEFAULTS = new Map([
  // This will be removed in the future.  If false, UnifiedComplete will not
  // perform any searches at all.
  ["autocomplete.enabled", true],

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

  // The amount of time (ms) to wait after the user has stopped typing before
  // fetching results.  However, we ignore this for the very first result (the
  // "heuristic" result).  We fetch it as fast as possible.
  ["delay", 50],

  // If true, this optimizes for replacing the full URL rather than selecting a
  // portion of it. This also copies the urlbar value to the selection
  // clipboard on systems that support it.
  ["doubleClickSelectsAll", false],

  // When true, `javascript:` URLs are not included in search results.
  ["filter.javascript", true],

  // Allows results from one search to be reused in the next search.  One of the
  // INSERTMETHOD values.
  ["insertMethod", UrlbarUtils.INSERTMETHOD.MERGE_RELATED],

  // Controls how URLs are matched against the user's search string.  See
  // mozIPlacesAutoComplete.
  ["matchBehavior", Ci.mozIPlacesAutoComplete.MATCH_BOUNDARY_ANYWHERE],

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

  // Results will include the user's bookmarks when this is true.
  ["suggest.bookmark", true],

  // Results will include the user's history when this is true.
  ["suggest.history", true],

  // Results will include the user's history when this is true, but only those
  // URLs with the "typed" flag (which includes but isn't limited to URLs the
  // user has typed in the urlbar).
  ["suggest.history.onlyTyped", false],

  // Results will include switch-to-tab results when this is true.
  ["suggest.openpage", true],

  // Results will include search suggestions when this is true.
  ["suggest.searches", false],

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
]);

const TYPES = [
  "history",
  "bookmark",
  "openpage",
  "searches",
];

const PREF_TYPES = new Map([
  ["boolean", "Bool"],
  ["string", "Char"],
  ["number", "Int"],
]);

// Buckets for match insertion.
// Every time a new match is returned, we go through each bucket in array order,
// and look for the first one having available space for the given match type.
// Each bucket is an array containing the following indices:
//   0: The match type of the acceptable entries.
//   1: available number of slots in this bucket.
// There are different matchBuckets definition for different contexts, currently
// a general one (matchBuckets) and a search one (matchBucketsSearch).
//
// First buckets. Anything with an Infinity frecency ends up here.
const DEFAULT_BUCKETS_BEFORE = [
  [UrlbarUtils.MATCH_GROUP.HEURISTIC, 1],
  [UrlbarUtils.MATCH_GROUP.EXTENSION, UrlbarUtils.MAXIMUM_ALLOWED_EXTENSION_MATCHES - 1],
];
// => USER DEFINED BUCKETS WILL BE INSERTED HERE <=
//
// Catch-all buckets. Anything remaining ends up here.
const DEFAULT_BUCKETS_AFTER = [
  [UrlbarUtils.MATCH_GROUP.SUGGESTION, Infinity],
  [UrlbarUtils.MATCH_GROUP.GENERAL, Infinity],
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

    Services.prefs.addObserver(PREF_URLBAR_BRANCH, this, true);
    Services.prefs.addObserver("keyword.enabled", this, true);

    // On startup we must check that some prefs are linked.
    this._updateLinkedPrefs();
  }

  /**
   * Returns the value for the preference with the given name.
   *
   * @param {string} pref
   *        The name of the preference to get.
   * @returns {*} The preference value.
   */
  get(pref) {
    if (!this._map.has(pref))
      this._map.set(pref, this._getPrefValue(pref));
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
    if (!PREF_URLBAR_DEFAULTS.has(pref) && !PREF_OTHER_DEFAULTS.has(pref))
      return;
    this._map.delete(pref);
    // Some prefs may influence others.
    if (pref == "matchBuckets") {
      this._map.delete("matchBucketsSearch");
    } else if (pref == "suggest.history") {
      this._map.delete("suggest.history.onlyTyped");
    }
    if (pref == "autocomplete.enabled" || pref.startsWith("suggest.")) {
      this._map.delete("defaultBehavior");
      this._map.delete("emptySearchDefaultBehavior");
      this._updateLinkedPrefs(pref);
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
    if (def === undefined)
      throw new Error("Trying to access an unknown pref " + pref);
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
          val = PlacesUtils.convertMatchBucketsStringToArray(PREF_URLBAR_DEFAULTS.get(pref));
        }
        return [ ...DEFAULT_BUCKETS_BEFORE,
                ...val,
                ...DEFAULT_BUCKETS_AFTER ];
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
            return [ ...DEFAULT_BUCKETS_BEFORE,
                    ...val,
                    ...DEFAULT_BUCKETS_AFTER ];
          } catch (ex) { /* invalid format, will just return matchBuckets */ }
        }
        return this.get("matchBuckets");
      }
      case "suggest.history.onlyTyped": {
        // If history is not set, onlyTyped value should be ignored.
        return this.get("suggest.history") && this._readPref(pref);
      }
      case "defaultBehavior": {
        let val = 0;
        for (let type of [...TYPES, "history.onlyTyped"]) {
          let behavior = type == "history.onlyTyped" ? "TYPED" : type.toUpperCase();
          val |= this.get("suggest." + type) &&
                 Ci.mozIPlacesAutoComplete["BEHAVIOR_" + behavior];
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
          val |= Ci.mozIPlacesAutoComplete.BEHAVIOR_HISTORY |
                Ci.mozIPlacesAutoComplete.BEHAVIOR_TYPED;
        } else if (this.get("suggest.bookmark")) {
          val |= Ci.mozIPlacesAutoComplete.BEHAVIOR_BOOKMARK;
        } else {
          val |= Ci.mozIPlacesAutoComplete.BEHAVIOR_OPENPAGE;
        }
        return val;
      }
      case "matchBehavior": {
        // Validate matchBehavior; default to MATCH_BOUNDARY_ANYWHERE.
        let val = this._readPref(pref);
        if (![Ci.mozIPlacesAutoComplete.MATCH_ANYWHERE,
              Ci.mozIPlacesAutoComplete.MATCH_BOUNDARY,
              Ci.mozIPlacesAutoComplete.MATCH_BEGINNING].includes(val)) {
          val = Ci.mozIPlacesAutoComplete.MATCH_BOUNDARY_ANYWHERE;
        }
        return val;
      }
    }
    return this._readPref(pref);
  }

  /**
   * Used to keep some pref values linked.
   * TODO: remove autocomplete.enabled and rely only on suggest.* prefs once we
   * can drop legacy add-ons compatibility.
   *
   * @param {string} changedPref
   *        The name of the preference that changed.
   */
  _updateLinkedPrefs(changedPref = "") {
    // Avoid re-entrance.
    if (this._linkingPrefs) {
      return;
    }
    this._linkingPrefs = true;
    try {
      let branch = Services.prefs.getBranch(PREF_URLBAR_BRANCH);
      if (changedPref.startsWith("suggest.")) {
        // A suggest pref changed, fix autocomplete.enabled.
        branch.setBoolPref("autocomplete.enabled",
                          TYPES.some(type => this.get("suggest." + type)));
      } else if (this.get("autocomplete.enabled")) {
        // If autocomplete is enabled and all of the suggest.* prefs are
        // disabled, reset the suggest.* prefs to their default value.
        if (TYPES.every(type => !this.get("suggest." + type))) {
          for (let type of TYPES) {
            let def = PREF_URLBAR_DEFAULTS.get("suggest." + type);
            branch.setBoolPref("suggest." + type, def);
          }
        }
      } else {
        // If autocomplete is disabled, deactivate all suggest preferences.
        for (let type of TYPES) {
          branch.setBoolPref("suggest." + type, false);
        }
      }
    } finally {
      delete this._linkingPrefs;
    }
  }

  /**
   * QueryInterface
   *
   * @param {IID} qiIID
   * @returns {Preferences} this
   */
  QueryInterface(qiIID) {
    let supportedIIDs = [
      Ci.nsISupports,
      Ci.nsIObserver,
      Ci.nsISupportsWeakReference,
    ];
    for (let iid of supportedIIDs) {
      if (Ci[iid].equals(qiIID)) {
        return this;
      }
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
}

var UrlbarPrefs = new Preferences();
