/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["PageDataService"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
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

const ALLOWED_SCHEMES = ["http", "https", "data", "blob"];

/**
 * @typedef {object} PageData
 *   A set of discovered from a page. Other than the `data` property this is the
 *   schema at `browser/components/pagedata/schemas/general.schema.json`.
 * @property {string} url
 *   The page's url.
 * @property {number} date
 *   The epoch based timestamp for when the data was discovered.
 * @property {string} siteName
 *   The page's friendly site name.
 * @property {string} image
 *   The page's image.
 * @property {object} data
 *   The map of data found which may be empty if no data was found. The key in
 *   map is from the `PageDataSchema.DATA_TYPE` enumeration. The values are in
 *   the format defined by the schemas at `browser/components/pagedata/schemas`.
 */

const PageDataService = new (class PageDataService extends EventEmitter {
  /**
   * Caches page data discovered from browsers. The key is the url of the data.
   *
   * TODO: Currently the cache never expires.
   *
   * @type {Map<string, PageData>}
   */
  #pageDataCache = new Map();

  /**
   * Initializes a new instance of the service, not called externally.
   */
  init() {
    if (!Services.prefs.getBoolPref("browser.pagedata.enabled", false)) {
      return;
    }

    ChromeUtils.registerWindowActor("PageData", {
      parent: {
        moduleURI: "resource:///actors/PageDataParent.jsm",
      },
      child: {
        moduleURI: "resource:///actors/PageDataChild.jsm",
        events: {
          DOMContentLoaded: {},
          pageshow: {},
        },
      },
    });

    logConsole.debug("Service started");

    for (let win of BrowserWindowTracker.orderedWindows) {
      if (!win.closed) {
        // Ask any existing tabs to report
        for (let tab of win.gBrowser.tabs) {
          let parent = tab.linkedBrowser.browsingContext?.currentWindowGlobal.getActor(
            "PageData"
          );

          parent.sendAsyncMessage("PageData:CheckLoaded");
        }
      }
    }
  }

  /**
   * Called when the service is destroyed. This is generally on shutdown so we
   * don't really need to do much cleanup.
   */
  uninit() {
    logConsole.debug("Service stopped");
  }

  /**
   * Called when the content process signals that a page is ready for data
   * collection.
   *
   * @param {PageDataParent} actor
   *   The parent actor for the page.
   * @param {string} url
   *   The url of the page.
   */
  async pageLoaded(actor, url) {
    let uri = Services.io.newURI(url);
    if (!ALLOWED_SCHEMES.includes(uri.scheme)) {
      return;
    }

    let browser = actor.browsingContext?.embedderElement;
    // If we don't have a browser then it went away before we could record,
    // so we don't know where the data came from.
    if (!browser || !this.#isATabBrowser(browser)) {
      return;
    }

    let data = await actor.collectPageData();
    this.pageDataDiscovered(data);
  }

  /**
   * Adds data for a url. This should generally only be called by other components of the
   * page data service or tests for simulating page data collection.
   *
   * @param {PageData} pageData
   *   The set of data discovered.
   */
  pageDataDiscovered(pageData) {
    logConsole.debug("Discovered page data", pageData);

    this.#pageDataCache.set(pageData.url, {
      ...pageData,
      data: pageData.data ?? {},
    });

    // Send out a notification.
    this.emit("page-data", pageData);
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
   * Queues page data retrieval for a url. The page-data notification will be
   * generated if data becomes available.
   *
   * Check `getCached` first to ensure that data is not already in the cache.
   *
   * @param {string} url
   *   The url to retrieve data for.
   */
  async queueFetch(url) {
    // Stub-implementation that generates an empty record.
    let pageData = {
      url,
      date: Date.now(),
      data: {},
    };

    this.#pageDataCache.set(url, pageData);

    // Send out a notification. The `no-page-data` notification is intended
    // for test use only.
    this.emit("page-data", pageData);
  }

  /**
   * Determines if the given browser is contained within a tab.
   *
   * @param {DOMElement} browser
   *   The browser element to check.
   * @returns {boolean}
   *   True if the browser element is contained within a tab.
   */
  #isATabBrowser(browser) {
    return browser.ownerGlobal.gBrowser?.getTabForBrowser(browser);
  }
})();
