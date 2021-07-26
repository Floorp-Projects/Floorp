/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["PageDataService"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
  EventEmitter: "resource://gre/modules/EventEmitter.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logConsole", function() {
  return console.createInstance({
    prefix: "PageData",
    maxLogLevel: Services.prefs.getBoolPref("browser.pagedata.log", false)
      ? "Debug"
      : "Warn",
  });
});

/**
 * @typedef {object} Data
 *   An individual piece of data about a page.
 * @property {string} type
 *   The type of data.
 * @property {object} data
 *   The data in a format specific to the type of data.
 *
 * @typedef {object} PageData
 *   A set of discovered from a page.
 * @property {string} url
 *   The page's url.
 * @property {number} date
 *   The epoch based timestamp for when the data was discovered.
 * @property {Data[]} data
 *   The array of data found which may be empty if no data was found.
 */

const PageDataService = new (class PageDataService extends EventEmitter {
  /**
   * Caches page data discovered from browsers. The key is the url of the data. Currently the cache
   * never expires.
   * @type {Map<string, PageData[]>}
   */
  #pageDataCache = new Map();

  /**
   * Constructs a new instance of the service, not called externally.
   */
  constructor() {
    super();

    if (!Services.prefs.getBoolPref("browser.pagedata.enabled", false)) {
      return;
    }

    logConsole.debug("Service started");
  }

  /**
   * Adds data for a url. This should generally only be called by other components of the
   * page data service or tests for simulating page data collection.
   *
   * @param {string} url
   *   The url of the page.
   * @param {Data[]} data
   *   The set of data discovered.
   */
  pageDataDiscovered(url, data) {
    let pageData = {
      url,
      date: Date.now(),
      data,
    };

    this.#pageDataCache.set(url, pageData);

    // Send out a notification if there was some data found.
    if (data.length) {
      this.emit("page-data", pageData);
    }
  }

  /**
   * Retrieves any cached page data. Returns null if there is no information in the cache, this will
   * happen either if the page has not been browsed recently or if data collection failed for some
   * reason.
   *
   * @param {string} url
   *   The url to retrieve data for.
   * @returns {PageData|null}
   *   A `PageData` if one is cached (it may not actually contain any items of data) or null if this
   *   page has not been successfully checked for data recently.
   */
  getCached(url) {
    return this.#pageDataCache.get(url) ?? null;
  }

  /**
   * Queues page data retrieval for a url.
   *
   * @param {string} url
   *   The url to retrieve data for.
   * @returns {Promise<PageData>}
   *   Resolves to a `PageData` (which may not contain any items of data) when the page has been
   *   successfully checked for data. Will resolve immediately if there is cached data available.
   *   Rejects if there was some failure to collect data.
   */
  async queueFetch(url) {
    let cached = this.#pageDataCache.get(url);
    if (cached) {
      return cached;
    }

    let pageData = {
      url,
      date: Date.now(),
      data: [],
    };

    this.#pageDataCache.set(url, pageData);

    // Send out a notification if there was some data found.
    if (pageData.data.length) {
      this.emit("page-data", pageData);
    }

    return pageData;
  }
})();
