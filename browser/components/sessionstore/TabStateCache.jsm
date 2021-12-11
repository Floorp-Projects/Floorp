/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TabStateCache"];

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
var TabStateCache = Object.freeze({
  /**
   * Retrieves cached data for a given |tab| or associated |browser|.
   *
   * @param permanentKey (object)
   *        The tab or browser to retrieve cached data for.
   * @return (object)
   *         The cached data stored for the given |tab|
   *         or associated |browser|.
   */
  get(permanentKey) {
    return TabStateCacheInternal.get(permanentKey);
  },

  /**
   * Updates cached data for a given |tab| or associated |browser|.
   *
   * @param permanentKey (object)
   *        The tab or browser belonging to the given tab data.
   * @param newData (object)
   *        The new data to be stored for the given |tab|
   *        or associated |browser|.
   */
  update(permanentKey, newData) {
    TabStateCacheInternal.update(permanentKey, newData);
  },
});

var TabStateCacheInternal = {
  _data: new WeakMap(),

  /**
   * Retrieves cached data for a given |tab| or associated |browser|.
   *
   * @param permanentKey (object)
   *        The tab or browser to retrieve cached data for.
   * @return (object)
   *         The cached data stored for the given |tab|
   *         or associated |browser|.
   */
  get(permanentKey) {
    return this._data.get(permanentKey);
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
  updatePartialStorageChange(data, change) {
    if (!data.storage) {
      data.storage = {};
    }

    let storage = data.storage;
    for (let domain of Object.keys(change)) {
      if (!change[domain]) {
        // We were sent null in place of the change object, which means
        // we should delete session storage entirely for this domain.
        delete storage[domain];
      } else {
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
  updatePartialHistoryChange(data, change) {
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
            history.entries,
            [start, toIdx - start].concat(change.entries)
          );
        }
      } else if (key != "fromIdx" && key != "toIdx") {
        history[key] = change[key];
      }
    }
  },

  /**
   * Helper function used by update (see below). To be fission compatible
   * we need to be able to update scroll and formdata per entry in the
   * cache. This is done by looking up the desired position and applying
   * the update for that node only.
   *
   * @param data (object)
   *        The cached data where we want to update the changes.
   * @param path (object)
   *        The path to the node to update specified by a list of indices
   *        to follow from the root downwards.
   * @param includeChildren (booelan)
   *        Determines if the children of the changed node should be kept
   *        or not.
   * @param change (object)
   *        Object containing the optional formdata and optional scroll
   *        position to be updated as well as information if the node
   *        should keep the data for its children.
   */
  updatePartialWindowStateChange(data, path, includeChildren, change) {
    if (!path.length) {
      for (let key of Object.keys(change)) {
        let children = includeChildren ? data[key]?.children : null;

        if (!Object.keys(change[key]).length) {
          data[key] = null;
        } else {
          data[key] = change[key];
        }

        if (children) {
          data[key] = { ...data[key], children };
        }
      }

      return data;
    }

    let index = path.pop();
    let scroll = data?.scroll?.children?.[index];
    let formdata = data?.formdata?.children?.[index];
    change = this.updatePartialWindowStateChange(
      { scroll, formdata },
      path,
      includeChildren,
      change
    );

    for (let key of Object.keys(change)) {
      let value = change[key];
      let children = data[key]?.children;

      if (children) {
        if (value) {
          children[index] = value;
        } else {
          delete children[index];
        }

        if (!children.some(e => e)) {
          data[key] = null;
        }
      } else if (value) {
        children = new Array(index + 1);
        children[index] = value;
        data[key] = { ...data[key], children };
      }
    }
    return data;
  },

  /**
   * Updates cached data for a given |tab| or associated |browser|.
   *
   * @param permanentKey (object)
   *        The tab or browser belonging to the given tab data.
   * @param newData (object)
   *        The new data to be stored for the given |tab|
   *        or associated |browser|.
   */
  update(permanentKey, newData) {
    let data = this._data.get(permanentKey) || {};

    for (let key of Object.keys(newData)) {
      if (key == "storagechange") {
        this.updatePartialStorageChange(data, newData.storagechange);
        continue;
      }

      if (key == "historychange") {
        this.updatePartialHistoryChange(data, newData.historychange);
        continue;
      }

      if (key == "windowstatechange") {
        let { path, hasChildren, ...change } = newData.windowstatechange;
        this.updatePartialWindowStateChange(data, path, hasChildren, change);

        for (key of Object.keys(change)) {
          let value = data[key];
          if (value === null) {
            delete data[key];
          } else {
            data[key] = value;
          }
        }

        continue;
      }

      let value = newData[key];
      if (value === null) {
        delete data[key];
      } else {
        data[key] = value;
      }
    }

    this._data.set(permanentKey, data);
  },
};
