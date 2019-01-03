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

  async fetchLayout() {
    const endpoint = this.config.layout_endpoint;
    if (!endpoint) {
      Cu.reportError("No endpoint configured for pocket, so could not fetch layout");
      return null;
    }
    try {
      const response = await fetch(endpoint, {credentials: "omit"});
      if (!response.ok) {
        // istanbul ignore next
        throw new Error(`Stories endpoint returned unexpected status: ${response.status}`);
      }
      return response.json();
    } catch (error) {
      // istanbul ignore next
      Cu.reportError(`Failed to fetch layout: ${error.message}`);
    }
    // istanbul ignore next
    return null;
  }

  async loadCachedData() {
    const cachedData = await this.cache.get() || {};

    let {layout: layoutResponse} = cachedData;
    if (!layoutResponse || !(Date.now() - layoutResponse._timestamp > LAYOUT_UPDATE_TIME)) {
      layoutResponse = await this.fetchLayout();
      if (layoutResponse && layoutResponse.layout) {
        layoutResponse._timestamp = Date.now();
        await this.cache.set("layout", layoutResponse);
      } else {
        Cu.reportError("No response for response.layout prop");
      }
    }

    if (layoutResponse && layoutResponse.layout) {
      this.store.dispatch(ac.BroadcastToContent({type: at.DISCOVERY_STREAM_LAYOUT_UPDATE, data: layoutResponse.layout}));
    }
  }

  async enable() {
    await this.loadCachedData();
    this.loaded = true;
  }

  async disable() {
    // Clear cache
    await this.cache.set("layout", {});
    // Reset reducer
    this.store.dispatch(ac.BroadcastToContent({type: at.DISCOVERY_STREAM_LAYOUT_UPDATE, data: []}));
    this.loaded = false;
  }

  async onPrefChange() {
    // Load data from all endpoints if our config.enabled = true.
    if (this.config.enabled) {
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
      case at.DISCOVERY_STREAM_CONFIG_CHANGE:
        // When the config pref changes, load or unload data as needed.
        await this.onPrefChange();
        break;
      case at.UNINIT:
        // When this feed is shutting down:
        this.uninitPrefs();
        break;
    }
  }
};

const EXPORTED_SYMBOLS = ["DiscoveryStreamFeed"];
