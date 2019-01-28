/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);
ChromeUtils.import("resource://gre/modules/Services.jsm");

const {actionTypes: at, actionCreators: ac} = ChromeUtils.import("resource://activity-stream/common/Actions.jsm", {});
const {PersistentCache} = ChromeUtils.import("resource://activity-stream/lib/PersistentCache.jsm", {});

const CACHE_KEY = "discovery_stream";
const LAYOUT_UPDATE_TIME = 30 * 60 * 1000; // 30 minutes
const COMPONENT_FEEDS_UPDATE_TIME = 30 * 60 * 1000; // 30 minutes
const SPOCS_FEEDS_UPDATE_TIME = 30 * 60 * 1000; // 30 minutes
const CONFIG_PREF_NAME = "browser.newtabpage.activity-stream.discoverystream.config";

this.DiscoveryStreamFeed = class DiscoveryStreamFeed {
  constructor() {
    // Internal state for checking if we've intialized all our data
    this.loaded = false;

    // Persistent cache for remote endpoint data.
    this.cache = new PersistentCache(CACHE_KEY, true);
    // Internal in-memory cache for parsing json prefs.
    this._prefCache = {};
  }

  get config() {
    if (this._prefCache.config) {
      return this._prefCache.config;
    }
    try {
      this._prefCache.config = JSON.parse(Services.prefs.getStringPref(CONFIG_PREF_NAME, ""));
    } catch (e) {
      // istanbul ignore next
      this._prefCache.config = {};
      // istanbul ignore next
      Cu.reportError(`Could not parse preference. Try resetting ${CONFIG_PREF_NAME} in about:config.`);
    }
    return this._prefCache.config;
  }

  get showSpocs() {
    // showSponsored is generally a use set spoc opt out,
    // show_spocs is generally a mozilla set value.
    return this.store.getState().Prefs.values.showSponsored && this.config.show_spocs;
  }

  setupPrefs() {
    Services.prefs.addObserver(CONFIG_PREF_NAME, this);
    // Send the initial state of the pref on our reducer
    this.store.dispatch(ac.BroadcastToContent({type: at.DISCOVERY_STREAM_CONFIG_SETUP, data: this.config}));
  }

  uninitPrefs() {
    Services.prefs.removeObserver(CONFIG_PREF_NAME, this);
    // Reset in-memory cache
    this._prefCache = {};
  }

  observe(aSubject, aTopic, aPrefName) {
    if (aPrefName === CONFIG_PREF_NAME) {
      this._prefCache.config = null;
      this.store.dispatch(ac.BroadcastToContent({type: at.DISCOVERY_STREAM_CONFIG_CHANGE, data: this.config}));
    }
  }

  async fetchFromEndpoint(endpoint) {
    if (!endpoint) {
      Cu.reportError("Tried to fetch endpoint but none was configured.");
      return null;
    }
    try {
      const response = await fetch(endpoint, {credentials: "omit"});
      if (!response.ok) {
        // istanbul ignore next
        throw new Error(`${endpoint} returned unexpected status: ${response.status}`);
      }
      return response.json();
    } catch (error) {
      // istanbul ignore next
      Cu.reportError(`Failed to fetch ${endpoint}: ${error.message}`);
    }
    // istanbul ignore next
    return null;
  }

  /**
   * Returns true if data in the cache for a particular key has expired or is missing.
   * @param {{}} cacheData data returned from cache.get()
   * @param {string} key a cache key
   * @param {string?} url for "feed" only, the URL of the feed.
   */
  isExpired(cacheData, key, url) {
    const {layout, spocs, feeds} = cacheData;
    switch (key) {
      case "layout":
        return (!layout || !(Date.now() - layout._timestamp < LAYOUT_UPDATE_TIME));
      case "spocs":
        return (!spocs || !(Date.now() - spocs.lastUpdated < SPOCS_FEEDS_UPDATE_TIME));
      case "feed":
        return (!feeds || !feeds[url] || !(Date.now() - feeds[url].lastUpdated < COMPONENT_FEEDS_UPDATE_TIME));
      default:
        throw new Error(`${key} is not a valid key`);
    }
  }

  /**
   * Returns true if any data for the cached endpoints has expired or is missing.
   */
  async checkIfAnyCacheExpired() {
    const cachedData = await this.cache.get() || {};
    const {feeds} = cachedData;
    return this.isExpired(cachedData, "layout") ||
      this.isExpired(cachedData, "spocs") ||
      !feeds || Object.keys(feeds).some(url => this.isExpired(cachedData, "feed", url));
  }

  async loadLayout(sendUpdate) {
    const cachedData = await this.cache.get() || {};
    let {layout: layoutResponse} = cachedData;
    if (this.isExpired(cachedData, "layout")) {
      layoutResponse = await this.fetchFromEndpoint(this.config.layout_endpoint);
      if (layoutResponse && layoutResponse.layout) {
        layoutResponse._timestamp = Date.now();
        await this.cache.set("layout", layoutResponse);
      } else {
        Cu.reportError("No response for response.layout prop");
      }
    }

    if (layoutResponse && layoutResponse.layout) {
      sendUpdate({
        type: at.DISCOVERY_STREAM_LAYOUT_UPDATE,
        data: {
          layout: layoutResponse.layout,
          lastUpdated: layoutResponse._timestamp,
        },
      });
    }
    if (layoutResponse && layoutResponse.spocs && layoutResponse.spocs.url) {
      sendUpdate({
        type: at.DISCOVERY_STREAM_SPOCS_ENDPOINT,
        data: layoutResponse.spocs.url,
      });
    }
  }

  async loadComponentFeeds(sendUpdate) {
    const {DiscoveryStream} = this.store.getState();
    const newFeeds = {};
    if (DiscoveryStream && DiscoveryStream.layout) {
      for (let row of DiscoveryStream.layout) {
        if (!row || !row.components) {
          continue;
        }
        for (let component of row.components) {
          if (component && component.feed) {
            const {url} = component.feed;
            newFeeds[url] = await this.getComponentFeed(url);
          }
        }
      }

      await this.cache.set("feeds", newFeeds);
      sendUpdate({type: at.DISCOVERY_STREAM_FEEDS_UPDATE, data: newFeeds});
    }
  }

  async loadSpocs(sendUpdate) {
    const cachedData = await this.cache.get() || {};
    let spocs;

    if (this.showSpocs) {
      spocs = cachedData.spocs;
      if (this.isExpired(cachedData, "spocs")) {
        const endpoint = this.store.getState().DiscoveryStream.spocs.spocs_endpoint;
        const spocsResponse = await this.fetchFromEndpoint(endpoint);
        if (spocsResponse) {
          spocs = {
            lastUpdated: Date.now(),
            data: spocsResponse,
          };
          await this.cache.set("spocs", spocs);
        } else {
          Cu.reportError("No response for spocs_endpoint prop");
        }
      }
    }

    // Use good data if we have it, otherwise nothing.
    // We can have no data if spocs set to off.
    // We can have no data if request fails and there is no good cache.
    // We want to send an update spocs or not, so client can render something.
    spocs = spocs || {
      lastUpdated: Date.now(),
      data: {},
    };

    sendUpdate({
      type: at.DISCOVERY_STREAM_SPOCS_UPDATE,
      data: {
        lastUpdated: spocs.lastUpdated,
        spocs: spocs.data,
      },
    });
  }

  async getComponentFeed(feedUrl) {
    const cachedData = await this.cache.get() || {};
    const {feeds} = cachedData;
    let feed = feeds ? feeds[feedUrl] : null;
    if (this.isExpired(cachedData, "feed", feedUrl)) {
      const feedResponse = await this.fetchFromEndpoint(feedUrl);
      if (feedResponse) {
        feed = {
          lastUpdated: Date.now(),
          data: feedResponse,
        };
      } else {
        Cu.reportError("No response for feed");
      }
    }

    return feed;
  }

  /**
   * @typedef {Object} RefreshAllOptions
   * @property {boolean} updateOpenTabs - Sends updates to open tabs immediately if true,
   *                                      updates in background if false

   * Refreshes layout, component feeds, and spocs in order if caches have expired.
   * @param {RefreshAllOptions} options
   */
  async refreshAll(options = {}) {
    const dispatch = options.updateOpenTabs ?
      action => this.store.dispatch(ac.BroadcastToContent(action)) :
      this.store.dispatch;

    await this.loadLayout(dispatch);
    await this.loadComponentFeeds(dispatch);
    await this.loadSpocs(dispatch);
    this.loaded = true;
  }

  async enable() {
    await this.refreshAll({updateOpenTabs: true});
  }

  async disable() {
    await this.clearCache();
    // Reset reducer
    this.store.dispatch(ac.BroadcastToContent({type: at.DISCOVERY_STREAM_LAYOUT_RESET}));
    this.loaded = false;
  }

  async clearCache() {
    await this.cache.set("layout", {});
    await this.cache.set("feeds", {});
    await this.cache.set("spocs", {});
  }

  async onPrefChange() {
    if (this.config.enabled) {
      // We always want to clear the cache if the pref has changed
      await this.clearCache();
      // Load data from all endpoints
      await this.enable();
    }

    // Clear state and relevant listeners if config.enabled = false.
    if (this.loaded && !this.config.enabled) {
      await this.disable();
    }
  }

  async onAction(action) {
    switch (action.type) {
      case at.INIT:
        // During the initialization of Firefox:
        // 1. Set-up listeners and initialize the redux state for config;
        this.setupPrefs();
        // 2. If config.enabled is true, start loading data.
        if (this.config.enabled) {
          await this.enable();
        }
        break;
      case at.SYSTEM_TICK:
        // Only refresh if we loaded once in .enable()
        if (this.config.enabled && this.loaded && await this.checkIfAnyCacheExpired()) {
          await this.refreshAll({updateOpenTabs: false});
        }
        break;
      case at.DISCOVERY_STREAM_CONFIG_SET_VALUE:
        Services.prefs.setStringPref(CONFIG_PREF_NAME, JSON.stringify({...this.config, [action.data.name]: action.data.value}));
        break;
      case at.DISCOVERY_STREAM_CONFIG_CHANGE:
        // When the config pref changes, load or unload data as needed.
        await this.onPrefChange();
        break;
      case at.UNINIT:
        // When this feed is shutting down:
        this.uninitPrefs();
        break;
      case at.PREF_CHANGED:
        // Check if spocs was disabled. Remove them if they were.
        if (action.data.name === "showSponsored") {
          await this.loadSpocs();
        }
        break;
    }
  }
};

const EXPORTED_SYMBOLS = ["DiscoveryStreamFeed"];
