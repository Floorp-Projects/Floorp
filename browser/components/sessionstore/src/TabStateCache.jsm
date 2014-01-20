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
   * Retrieves cached data for a given |browser|.
   *
   * @param browser (xul:browser)
   *        The browser to retrieve cached data for.
   * @return (object)
   *         The cached data stored for the given |browser|.
   */
  get: function (browser) {
    return TabStateCacheInternal.get(browser);
  },

  /**
   * Updates cached data for a given |browser|.
   *
   * @param browser (xul:browser)
   *        The browser belonging to the given tab data.
   * @param newData (object)
   *        The new data to be stored for the given |browser|.
   */
  update: function (browser, newData) {
    TabStateCacheInternal.update(browser, newData);
  }
});

let TabStateCacheInternal = {
  _data: new WeakMap(),

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
    Utils.swapMapEntries(this._data, browser, otherBrowser);
  },

  /**
   * Retrieves cached data for a given |browser|.
   *
   * @param browser (xul:browser)
   *        The browser to retrieve cached data for.
   * @return (object)
   *         The cached data stored for the given |browser|.
   */
  get: function (browser) {
    return this._data.get(browser);
  },

  /**
   * Updates cached data for a given |browser|.
   *
   * @param browser (xul:browser)
   *        The browser belonging to the given tab data.
   * @param newData (object)
   *        The new data to be stored for the given |browser|.
   */
  update: function (browser, newData) {
    let data = this._data.get(browser) || {};

    for (let key of Object.keys(newData)) {
      let value = newData[key];
      if (value === null) {
        delete data[key];
      } else {
        data[key] = value;
      }
    }

    this._data.set(browser, data);
  }
};
