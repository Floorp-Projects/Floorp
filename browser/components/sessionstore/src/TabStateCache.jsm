/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["TabStateCache"];

const Cu = Components.utils;
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);

XPCOMUtils.defineLazyModuleGetter(this, "Utils",
  "resource:///modules/sessionstore/Utils.jsm");

/**
 * A cache for tabs data.
 *
 * This cache implements a weak map from tabs (as XUL elements)
 * to tab data (as objects).
 *
 * Note that we should never cache private data, as:
 * - that data is used very seldom by SessionStore;
 * - caching private data in addition to public data is memory consuming.
 */
this.TabStateCache = Object.freeze({
  /**
   * Tells whether an entry is in the cache.
   *
   * @param {XULElement} aKey The tab or the associated browser.
   * @return {bool} Whether there's a cached entry for the given tab.
   */
  has: function (aTab) {
    return TabStateCacheInternal.has(aTab);
  },

  /**
   * Add or replace an entry in the cache.
   *
   * @param {XULElement} aTab The key, which may be either a tab
   * or the corresponding browser. The binding will disappear
   * if the tab/browser is destroyed.
   * @param {*} aValue The data associated to |aTab|.
   */
  set: function(aTab, aValue) {
    return TabStateCacheInternal.set(aTab, aValue);
  },

  /**
   * Return the tab data associated with a tab.
   *
   * @param {XULElement} aKey The tab or the associated browser.
   *
   * @return {*|undefined} The data if available, |undefined|
   * otherwise.
   */
  get: function(aKey) {
    return TabStateCacheInternal.get(aKey);
  },

  /**
   * Delete the tab data associated with a tab.
   *
   * @param {XULElement} aKey The tab or the associated browser.
   *
   * Noop of there is no tab data associated with the tab.
   */
  delete: function(aKey) {
    return TabStateCacheInternal.delete(aKey);
  },

  /**
   * Delete all tab data.
   */
  clear: function() {
    return TabStateCacheInternal.clear();
  },

  /**
   * Update in place a piece of data.
   *
   * @param {XULElement} aKey The tab or the associated browser.
   * If the tab/browser is not present, do nothing.
   * @param {string} aField The field to update.
   * @param {*} aValue The new value to place in the field.
   */
  updateField: function(aKey, aField, aValue) {
    return TabStateCacheInternal.updateField(aKey, aField, aValue);
  },

  /**
   * Remove a given field from a cached tab state.
   *
   * @param {XULElement} aKey The tab or the associated browser.
   * If the tab/browser is not present, do nothing.
   * @param {string} aField The field to remove.
   */
  removeField: function(aKey, aField) {
    return TabStateCacheInternal.removeField(aKey, aField);
  },

  /**
   * Swap cached data for two given browsers.
   *
   * @param {xul:browser} browser
   *        The first of the two browsers that swapped docShells.
   * @param {xul:browser} otherBrowser
   *        The second of the two browsers that swapped docShells.
   */
  onBrowserContentsSwapped: function(browser, otherBrowser) {
    TabStateCacheInternal.onBrowserContentsSwapped(browser, otherBrowser);
  },

  /**
   * Retrieves persistently cached data for a given |browser|.
   *
   * @param browser (xul:browser)
   *        The browser to retrieve cached data for.
   * @return (object)
   *         The persistently cached data stored for the given |browser|.
   */
  getPersistent: function (browser) {
    return TabStateCacheInternal.getPersistent(browser);
  },

  /**
   * Updates persistently cached data for a given |browser|. This data is
   * persistently in the sense that we never clear it, it will always be
   * overwritten.
   *
   * @param browser (xul:browser)
   *        The browser belonging to the given tab data.
   * @param newData (object)
   *        The new data to be stored for the given |browser|.
   */
  updatePersistent: function (browser, newData) {
    TabStateCacheInternal.updatePersistent(browser, newData);
  },

  /**
   * Total number of cache hits during the session.
   */
  get hits() {
    return TabStateCacheTelemetry.hits;
  },

  /**
   * Total number of cache misses during the session.
   */
  get misses() {
    return TabStateCacheTelemetry.misses;
  },

  /**
   * Total number of cache clears during the session.
   */
  get clears() {
    return TabStateCacheTelemetry.clears;
  },
});

let TabStateCacheInternal = {
  _data: new WeakMap(),
  _persistentData: new WeakMap(),

  /**
   * Tells whether an entry is in the cache.
   *
   * @param {XULElement} aKey The tab or the associated browser.
   * @return {bool} Whether there's a cached entry for the given tab.
   */
  has: function (aTab) {
    let key = this._normalizeToBrowser(aTab);
    return this._data.has(key);
  },

  /**
   * Add or replace an entry in the cache.
   *
   * @param {XULElement} aTab The key, which may be either a tab
   * or the corresponding browser. The binding will disappear
   * if the tab/browser is destroyed.
   * @param {*} aValue The data associated to |aTab|.
   */
  set: function(aTab, aValue) {
    let key = this._normalizeToBrowser(aTab);
    this._data.set(key, aValue);
  },

  /**
   * Return the tab data associated with a tab.
   *
   * @param {XULElement} aKey The tab or the associated browser.
   *
   * @return {*|undefined} The data if available, |undefined|
   * otherwise.
   */
  get: function(aKey) {
    let key = this._normalizeToBrowser(aKey);
    let result = this._data.get(key);
    TabStateCacheTelemetry.recordAccess(!!result);
    return result;
  },

  /**
   * Delete the tab data associated with a tab.
   *
   * @param {XULElement} aKey The tab or the associated browser.
   *
   * Noop of there is no tab data associated with the tab.
   */
  delete: function(aKey) {
    let key = this._normalizeToBrowser(aKey);
    this._data.delete(key);
  },

  /**
   * Delete all tab data.
   */
  clear: function() {
    TabStateCacheTelemetry.recordClear();
    this._data.clear();
  },

  /**
   * Update in place a piece of data.
   *
   * @param {XULElement} aKey The tab or the associated browser.
   * If the tab/browser is not present, do nothing.
   * @param {string} aField The field to update.
   * @param {*} aValue The new value to place in the field.
   */
  updateField: function(aKey, aField, aValue) {
    let key = this._normalizeToBrowser(aKey);
    let data = this._data.get(key);
    if (data) {
      data[aField] = aValue;
    }
  },

  /**
   * Remove a given field from a cached tab state.
   *
   * @param {XULElement} aKey The tab or the associated browser.
   * If the tab/browser is not present, do nothing.
   * @param {string} aField The field to remove.
   */
  removeField: function(aKey, aField) {
    let key = this._normalizeToBrowser(aKey);
    let data = this._data.get(key);
    if (data && aField in data) {
      delete data[aField];
    }
  },

  /**
   * Swap cached data for two given browsers.
   *
   * @param {xul:browser} browser
   *        The first of the two browsers that swapped docShells.
   * @param {xul:browser} otherBrowser
   *        The second of the two browsers that swapped docShells.
   */
  onBrowserContentsSwapped: function(browser, otherBrowser) {
    // Swap data stored per-browser.
    [this._data, this._persistentData]
      .forEach(map => Utils.swapMapEntries(map, browser, otherBrowser));
  },

  /**
   * Retrieves persistently cached data for a given |browser|.
   *
   * @param browser (xul:browser)
   *        The browser to retrieve cached data for.
   * @return (object)
   *         The persistently cached data stored for the given |browser|.
   */
  getPersistent: function (browser) {
    return this._persistentData.get(browser);
  },

  /**
   * Updates persistently cached data for a given |browser|. This data is
   * persistent in the sense that we never clear it, it will always be
   * overwritten.
   *
   * @param browser (xul:browser)
   *        The browser belonging to the given tab data.
   * @param newData (object)
   *        The new data to be stored for the given |browser|.
   */
  updatePersistent: function (browser, newData) {
    let data = this._persistentData.get(browser) || {};

    for (let key of Object.keys(newData)) {
      let value = newData[key];
      if (value === null) {
        delete data[key];
      } else {
        data[key] = value;
      }
    }

    this._persistentData.set(browser, data);
  },

  _normalizeToBrowser: function(aKey) {
    let nodeName = aKey.localName;
    if (nodeName == "tab") {
      return aKey.linkedBrowser;
    }
    if (nodeName == "browser") {
      return aKey;
    }
    throw new TypeError("Key is neither a tab nor a browser: " + nodeName);
  }
};

let TabStateCacheTelemetry = {
  // Total number of hits during the session
  hits: 0,
  // Total number of misses during the session
  misses: 0,
  // Total number of clears during the session
  clears: 0,
  // |true| once we have been initialized
  _initialized: false,

  /**
   * Record a cache access.
   *
   * @param {boolean} isHit If |true|, the access was a hit, otherwise
   * a miss.
   */
  recordAccess: function(isHit) {
    this._init();
    if (isHit) {
      ++this.hits;
    } else {
      ++this.misses;
    }
  },

  /**
   * Record a cache clear
   */
  recordClear: function() {
    this._init();
    ++this.clears;
  },

  /**
   * Initialize the telemetry.
   */
  _init: function() {
    if (this._initialized) {
      // Avoid double initialization
      return;
    }
    this._initialized = true;
    Services.obs.addObserver(this, "profile-before-change", false);
  },

  observe: function() {
    Services.obs.removeObserver(this, "profile-before-change");

    // Record hit/miss rate
    let accesses = this.hits + this.misses;
    if (accesses == 0) {
      return;
    }

    this._fillHistogram("HIT_RATE", this.hits, accesses);
    this._fillHistogram("CLEAR_RATIO", this.clears, accesses);
  },

  _fillHistogram: function(suffix, positive, total) {
    let PREFIX = "FX_SESSION_RESTORE_TABSTATECACHE_";
    let histo = Services.telemetry.getHistogramById(PREFIX + suffix);
    let rate = Math.floor( ( positive * 100 ) / total );
    histo.add(rate);
  }
};
