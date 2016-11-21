/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["TabStateCache"];

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
   * Retrieves cached data for a given |tab| or associated |browser|.
   *
   * @param browserOrTab (xul:tab or xul:browser)
   *        The tab or browser to retrieve cached data for.
   * @return (object)
   *         The cached data stored for the given |tab|
   *         or associated |browser|.
   */
  get: function (browserOrTab) {
    return TabStateCacheInternal.get(browserOrTab);
  },

  /**
   * Updates cached data for a given |tab| or associated |browser|.
   *
   * @param browserOrTab (xul:tab or xul:browser)
   *        The tab or browser belonging to the given tab data.
   * @param newData (object)
   *        The new data to be stored for the given |tab|
   *        or associated |browser|.
   */
  update: function (browserOrTab, newData) {
    TabStateCacheInternal.update(browserOrTab, newData);
  }
});

var TabStateCacheInternal = {
  _data: new WeakMap(),

  /**
   * Retrieves cached data for a given |tab| or associated |browser|.
   *
   * @param browserOrTab (xul:tab or xul:browser)
   *        The tab or browser to retrieve cached data for.
   * @return (object)
   *         The cached data stored for the given |tab|
   *         or associated |browser|.
   */
  get: function (browserOrTab) {
    return this._data.get(browserOrTab.permanentKey);
  },

  /**
   * Helper function used by update (see below). For message size
   * optimization sometimes we don't update the whole session storage
   * only the values that have been changed.
   *
   * @param data (object)
   *        The cached data where we want to update the changes.
   * @param change (object)
   *        The actual changed values per domain.
   */
  updatePartialStorageChange: function (data, change) {
    if (!data.storage) {
      data.storage = {};
    }

    let storage = data.storage;
    for (let domain of Object.keys(change)) {
      for (let key of Object.keys(change[domain])) {
        let value = change[domain][key];
        if (value === null) {
          if (storage[domain] && storage[domain][key]) {
            delete storage[domain][key];
          }
        } else {
          if (!storage[domain]) {
            storage[domain] = {};
          }
          storage[domain][key] = value;
        }
      }
    }
  },

  /**
   * Helper function used by update (see below). For message size
   * optimization sometimes we don't update the whole browser history
   * only the current index and the tail of the history from a certain
   * index (specified by change.fromIdx)
   *
   * @param data (object)
   *        The cached data where we want to update the changes.
   * @param change (object)
   *        Object containing the tail of the history array, and
   *        some additional metadata.
   */
  updatePartialHistoryChange: function (data, change) {
    const kLastIndex = Number.MAX_SAFE_INTEGER - 1;

    if (!data.history) {
      data.history = { entries: [] };
    }

    let history = data.history;
    let toIdx = history.entries.length;
    if ("toIdx" in change) {
      toIdx = Math.min(toIdx, change.toIdx + 1);
    }

    for (let key of Object.keys(change)) {
      if (key == "entries") {
        if (change.fromIdx != kLastIndex) {
          let start = change.fromIdx + 1;
          history.entries.splice.apply(
            history.entries, [start, toIdx - start].concat(change.entries));
        }
      } else if (key != "fromIdx" && key != "toIdx") {
        history[key] = change[key];
      }
    }
  },

  /**
   * Updates cached data for a given |tab| or associated |browser|.
   *
   * @param browserOrTab (xul:tab or xul:browser)
   *        The tab or browser belonging to the given tab data.
   * @param newData (object)
   *        The new data to be stored for the given |tab|
   *        or associated |browser|.
   */
  update: function (browserOrTab, newData) {
    let data = this._data.get(browserOrTab.permanentKey) || {};

    for (let key of Object.keys(newData)) {
      if (key == "storagechange") {
        this.updatePartialStorageChange(data, newData.storagechange);
        continue;
      }

      if (key == "historychange") {
        this.updatePartialHistoryChange(data, newData.historychange);
        continue;
      }

      let value = newData[key];
      if (value === null) {
        delete data[key];
      } else {
        data[key] = value;
      }
    }

    this._data.set(browserOrTab.permanentKey, data);
  }
};
