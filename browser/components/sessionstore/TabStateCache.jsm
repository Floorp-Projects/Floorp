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
