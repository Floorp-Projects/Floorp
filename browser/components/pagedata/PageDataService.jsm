/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["PageDataService"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
  HiddenFrame: "resource://gre/modules/HiddenFrame.jsm",
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logConsole", function() {
  return console.createInstance({
    prefix: "PageData",
    maxLogLevel: Services.prefs.getBoolPref("browser.pagedata.log", false)
      ? "Debug"
      : "Warn",
  });
});

XPCOMUtils.defineLazyServiceGetters(lazy, {
  idleService: ["@mozilla.org/widget/useridleservice;1", "nsIUserIdleService"],
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "fetchIdleTime",
  "browser.pagedata.fetchIdleTime",
  300
);

const ALLOWED_SCHEMES = ["http", "https", "data", "blob"];

const BACKGROUND_WIDTH = 1024;
const BACKGROUND_HEIGHT = 768;

/**
 * Shifts the first element out of the set.
 *
 * @param {Set<T>} set
 *   The set containing elements.
 * @returns {T | undefined} The first element in the set or undefined if
 *   there is nothing in the set.
 */
function shift(set) {
  let iter = set.values();
  let { value, done } = iter.next();

  if (done) {
    return undefined;
  }

  set.delete(value);
  return value;
}

/**
 * A manager for hidden browsers. Responsible for creating and destroying a
 * hidden frame to hold them.
 */
class HiddenBrowserManager {
  /**
   * The hidden frame if one has been created.
   * @type {HiddenFrame | null}
   */
  #frame = null;
  /**
   * The number of hidden browser elements currently in use.
   * @type {number}
   */
  #browsers = 0;

  /**
   * Creates and returns a new hidden browser.
   *
   * @returns {Browser}
   */
  async #acquireBrowser() {
    this.#browsers++;
    if (!this.#frame) {
      this.#frame = new lazy.HiddenFrame();
    }

    let frame = await this.#frame.get();
    let doc = frame.document;
    let browser = doc.createXULElement("browser");
    browser.setAttribute("remote", "true");
    browser.setAttribute("type", "content");
    browser.setAttribute(
      "style",
      `
        width: ${BACKGROUND_WIDTH}px;
        min-width: ${BACKGROUND_WIDTH}px;
        height: ${BACKGROUND_HEIGHT}px;
        min-height: ${BACKGROUND_HEIGHT}px;
      `
    );
    browser.setAttribute("maychangeremoteness", "true");
    doc.documentElement.appendChild(browser);

    return browser;
  }

  /**
   * Releases the given hidden browser.
   *
   * @param {Browser} browser
   *   The hidden browser element.
   */
  #releaseBrowser(browser) {
    browser.remove();

    this.#browsers--;
    if (this.#browsers == 0) {
      this.#frame.destroy();
      this.#frame = null;
    }
  }

  /**
   * Calls a callback function with a new hidden browser.
   * This function will return whatever the callback function returns.
   *
   * @param {Callback} callback
   *   The callback function will be called with the browser element and may
   *   be asynchronous.
   * @returns {T}
   */
  async withHiddenBrowser(callback) {
    let browser = await this.#acquireBrowser();
    try {
      return await callback(browser);
    } finally {
      this.#releaseBrowser(browser);
    }
  }
}

/**
 * @typedef {object} CacheEntry
 *   An entry in the page data cache.
 * @property {PageData | null} pageData
 *   The data or null if there is no known data.
 * @property {Set} actors
 *   The actors that maintain an interest in keeping the entry cached.
 */

/**
 * A cache of page data kept in memory. By default any discovered data from
 * browsers is kept in memory until the browser element is destroyed but other
 * actors may register an interest in keeping an entry alive beyond that.
 */
class PageDataCache {
  /**
   * The contents of the cache. Keyed on page url.
   *
   * @type {Map<string, CacheEntry>}
   */
  #cache = new Map();

  /**
   * Creates or updates an entry in the cache. If no actor has registered any
   * interest in keeping this page's data in memory then this will do nothing.
   *
   * @param {string} url
   *   The url of the page.
   * @param {PageData|null} pageData
   *   The current page data for the page.
   */
  set(url, pageData) {
    let entry = this.#cache.get(url);

    if (entry) {
      entry.pageData = pageData;
    }
  }

  /**
   * Gets any cached data for the url.
   *
   * @param {string} url
   *   The url of the page.
   * @returns {PageData | null}
   *   The page data if some is known.
   */
  get(url) {
    let entry = this.#cache.get(url);
    return entry?.pageData ?? null;
  }

  /**
   * Adds a lock to an entry. This can be called before we have discovered the
   * data for the url.
   *
   * @param {object} actor
   *   Ensures the entry stays in memory until unlocked by this actor.
   * @param {string} url
   *   The url of the page.
   */
  lockData(actor, url) {
    let entry = this.#cache.get(url);
    if (entry) {
      entry.actors.add(actor);
    } else {
      this.#cache.set(url, {
        pageData: undefined,
        actors: new Set([actor]),
      });
    }
  }

  /**
   * Removes a lock from an entry.
   *
   * @param {object} actor
   *   The lock to remove.
   * @param {string | undefined} [url]
   *   The url of the page or undefined to unlock all urls locked by this actor.
   */
  unlockData(actor, url) {
    let entries = [];
    if (url) {
      let entry = this.#cache.get(url);
      if (!entry) {
        return;
      }

      entries.push([url, entry]);
    } else {
      entries = [...this.#cache];
    }

    for (let [entryUrl, entry] of entries) {
      if (entry.actors.delete(actor)) {
        if (entry.actors.size == 0) {
          this.#cache.delete(entryUrl);
        }
      }
    }
  }
}

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
   * Caches page data discovered from browsers.
   *
   * @type {PageDataCache}
   */
  #pageDataCache = new PageDataCache();

  /**
   * The number of currently running background fetches.
   * @type {number}
   */
  #backgroundFetches = 0;

  /**
   * The list of urls waiting to be loaded in the background.
   * @type {Set<string>}
   */
  #backgroundQueue = new Set();

  /**
   * Tracks whether the user is currently idle.
   * @type {boolean}
   */
  #userIsIdle = false;

  /**
   * A manager for hidden browsers.
   * @type {HiddenBrowserManager}
   */
  #browserManager = new HiddenBrowserManager();

  /**
   * A map of hidden browsers to a resolve function that should be passed the
   * actor that was created for the browser.
   *
   * @type {WeakMap<Browser, (actor: PageDataParent) => void>}
   */
  #backgroundBrowsers = new WeakMap();

  /**
   * Tracks windows that have browsers with entries in the cache.
   *
   * @type {Map<Window, Set<Browser>>}
   */
  #trackedWindows = new Map();

  /**
   * Constructs the service.
   */
  constructor() {
    super();

    // Limits the number of background fetches that will run at once. Set to 0 to
    // effectively allow an infinite number.
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "MAX_BACKGROUND_FETCHES",
      "browser.pagedata.maxBackgroundFetches",
      5,
      () => this.#startBackgroundWorkers()
    );
  }

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

    lazy.logConsole.debug("Service started");

    for (let win of lazy.BrowserWindowTracker.orderedWindows) {
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

    lazy.idleService.addIdleObserver(this, lazy.fetchIdleTime);
  }

  /**
   * Called when the service is destroyed. This is generally on shutdown so we
   * don't really need to do much cleanup.
   */
  uninit() {
    lazy.logConsole.debug("Service stopped");
  }

  /**
   * Starts tracking for when a browser is destroyed.
   *
   * @param {Browser} browser
   *   The browser to track.
   */
  #trackBrowser(browser) {
    let window = browser.ownerGlobal;

    let browsers = this.#trackedWindows.get(window);
    if (browsers) {
      browsers.add(browser);

      // This window is already being tracked, no need to add listeners.
      return;
    }

    browsers = new Set([browser]);
    this.#trackedWindows.set(window, browsers);

    window.addEventListener("unload", () => {
      for (let closedBrowser of browsers) {
        this.unlockEntry(closedBrowser);
      }

      this.#trackedWindows.delete(window);
    });

    window.addEventListener("TabClose", ({ target: tab }) => {
      // Unlock any entries locked by this browser.
      let closedBrowser = tab.linkedBrowser;
      this.unlockEntry(closedBrowser);
      browsers.delete(closedBrowser);
    });
  }

  /**
   * Requests that any page data for this url is retained in memory until
   * unlocked. By calling this you are committing to later call `unlockEntry`
   * with the same `actor` and `url` parameters.
   *
   * @param {object} actor
   *   The actor requesting the lock.
   * @param {string} url
   *   The url of the page to lock.
   */
  lockEntry(actor, url) {
    this.#pageDataCache.lockData(actor, url);
  }

  /**
   * Notifies that an actor is no longer interested in a url.
   *
   * @param {object} actor
   *   The actor that requested the lock.
   * @param {string | undefined} [url]
   *   The url of the page or undefined to unlock all urls locked by this actor.
   */
  unlockEntry(actor, url) {
    this.#pageDataCache.unlockData(actor, url);
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
    if (!browser) {
      return;
    }

    // Is this a load in a background browser?
    let backgroundResolve = this.#backgroundBrowsers.get(browser);
    if (backgroundResolve) {
      backgroundResolve(actor);
      return;
    }

    // Otherwise we only care about pages loaded in the tab browser.
    if (!this.#isATabBrowser(browser)) {
      return;
    }

    try {
      let data = await actor.collectPageData();
      if (data) {
        // Keep this data alive until the browser is destroyed.
        this.#trackBrowser(browser);
        this.lockEntry(browser, data.url);

        this.pageDataDiscovered(data);
      }
    } catch (e) {
      lazy.logConsole.error(e);
    }
  }

  /**
   * Adds data for a url. This should generally only be called by other components of the
   * page data service or tests for simulating page data collection.
   *
   * @param {PageData} pageData
   *   The set of data discovered.
   */
  pageDataDiscovered(pageData) {
    lazy.logConsole.debug("Discovered page data", pageData);

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
    return this.#pageDataCache.get(url);
  }

  /**
   * Fetches page data from the given URL using a hidden window. Note that this does not populate
   * the page data cache or emit the `page-data` event.
   *
   * @param {string} url
   *   The url to retrieve data for.
   * @returns {Promise<PageData|null>}
   *   Resolves to the found pagedata or null in case of error.
   */
  async fetchPageData(url) {
    return this.#browserManager.withHiddenBrowser(async browser => {
      try {
        let { promise, resolve } = lazy.PromiseUtils.defer();
        this.#backgroundBrowsers.set(browser, resolve);

        let principal = Services.scriptSecurityManager.getSystemPrincipal();
        let oa = lazy.E10SUtils.predictOriginAttributes({
          browser,
        });
        let loadURIOptions = {
          triggeringPrincipal: principal,
          remoteType: lazy.E10SUtils.getRemoteTypeForURI(
            url,
            true,
            false,
            lazy.E10SUtils.DEFAULT_REMOTE_TYPE,
            null,
            oa
          ),
        };
        browser.loadURI(url, loadURIOptions);

        let actor = await promise;
        return await actor.collectPageData();
      } finally {
        this.#backgroundBrowsers.delete(browser);
      }
    });
  }

  /**
   * Handles notifications from the idle service.
   *
   * @param {nsISupports} subject
   *   The notification's subject.
   * @param {string} topic
   *   The notification topic.
   * @param {string} data
   *   The data associated with the notification.
   */
  observe(subject, topic, data) {
    switch (topic) {
      case "idle":
        lazy.logConsole.debug("User went idle");
        this.#userIsIdle = true;
        this.#startBackgroundWorkers();
        break;
      case "active":
        lazy.logConsole.debug("User became active");
        this.#userIsIdle = false;
        break;
    }
  }

  /**
   * Starts as many background workers as are allowed to process the background
   * queue.
   */
  #startBackgroundWorkers() {
    if (!this.#userIsIdle) {
      return;
    }

    let toStart;

    if (this.MAX_BACKGROUND_FETCHES) {
      toStart = this.MAX_BACKGROUND_FETCHES - this.#backgroundFetches;
    } else {
      toStart = this.#backgroundQueue.size;
    }

    for (let i = 0; i < toStart; i++) {
      this.#backgroundFetch();
    }
  }

  /**
   * Starts a background fetch worker which will pull urls from the queue and
   * load them until the queue is empty.
   */
  async #backgroundFetch() {
    this.#backgroundFetches++;

    let url = shift(this.#backgroundQueue);
    while (url) {
      try {
        let pageData = await this.fetchPageData(url);

        if (pageData) {
          this.#pageDataCache.set(url, pageData);
          this.emit("page-data", pageData);
        }
      } catch (e) {
        lazy.logConsole.error(e);
      }

      // Check whether the user became active or the worker limit changed
      // dynamically.
      if (
        !this.#userIsIdle ||
        (this.MAX_BACKGROUND_FETCHES > 0 &&
          this.#backgroundFetches > this.MAX_BACKGROUND_FETCHES)
      ) {
        break;
      }

      url = shift(this.#backgroundQueue);
    }

    this.#backgroundFetches--;
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
  queueFetch(url) {
    this.#backgroundQueue.add(url);

    this.#startBackgroundWorkers();
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
