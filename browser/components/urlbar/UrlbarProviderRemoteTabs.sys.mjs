/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a provider that offers remote tabs.
 */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

import {
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  SyncedTabs: "resource://services-sync/SyncedTabs.jsm",
});

let _cache = null;

// By default, we add remote tabs that have been used more recently than this
// time ago. Any remaining remote tabs are added in queue if no other results
// are found.
const RECENT_REMOTE_TAB_THRESHOLD_MS = 72 * 60 * 60 * 1000; // 72 hours.

XPCOMUtils.defineLazyGetter(lazy, "weaveXPCService", function() {
  try {
    return Cc["@mozilla.org/weave/service;1"].getService(
      Ci.nsISupports
    ).wrappedJSObject;
  } catch (ex) {
    // The app didn't build Sync.
  }
  return null;
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "showRemoteIconsPref",
  "services.sync.syncedTabs.showRemoteIcons",
  true
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "showRemoteTabsPref",
  "services.sync.syncedTabs.showRemoteTabs",
  true
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "syncUsernamePref",
  "services.sync.username"
);

// from MDN...
function escapeRegExp(string) {
  return string.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

/**
 * Class used to create the provider.
 */
class ProviderRemoteTabs extends UrlbarProvider {
  constructor() {
    super();
    Services.obs.addObserver(this.observe, "weave:engine:sync:finish");
    Services.obs.addObserver(this.observe, "weave:service:start-over");
  }

  /**
   * Unique name for the provider, used by the context to filter on providers.
   */
  get name() {
    return "RemoteTabs";
  }

  /**
   * The type of the provider, must be one of UrlbarUtils.PROVIDER_TYPE.
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.NETWORK;
  }

  /**
   * Whether this provider should be invoked for the given context.
   * If this method returns false, the providers manager won't start a query
   * with this provider, to save on resources.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {boolean} Whether this provider should be invoked for the search.
   */
  isActive(queryContext) {
    return (
      lazy.syncUsernamePref &&
      lazy.showRemoteTabsPref &&
      lazy.UrlbarPrefs.get("suggest.remotetab") &&
      queryContext.sources.includes(UrlbarUtils.RESULT_SOURCE.TABS) &&
      lazy.weaveXPCService &&
      lazy.weaveXPCService.ready &&
      lazy.weaveXPCService.enabled
    );
  }

  /**
   * Starts querying.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @param {function} addCallback Callback invoked by the provider to add a new
   *        result. A UrlbarResult should be passed to it.
   * @note Extended classes should return a Promise resolved when the provider
   *       is done searching AND returning results.
   */
  async startQuery(queryContext, addCallback) {
    let instance = this.queryInstance;

    let searchString = queryContext.tokens.map(t => t.value).join(" ");

    let re = new RegExp(escapeRegExp(searchString), "i");
    let tabsData = await this.ensureCache();
    if (instance != this.queryInstance) {
      return;
    }

    let resultsAdded = 0;
    let staleTabs = [];
    for (let { tab, client } of tabsData) {
      if (
        !searchString ||
        searchString == lazy.UrlbarTokenizer.RESTRICT.OPENPAGE ||
        re.test(tab.url) ||
        (tab.title && re.test(tab.title))
      ) {
        if (lazy.showRemoteIconsPref) {
          if (!tab.icon) {
            // It's rare that Sync supplies the icon for the page. If it does, it is a
            // string URL.
            tab.icon = UrlbarUtils.getIconForUrl(tab.url);
          } else {
            tab.icon = lazy.PlacesUtils.favicons.getFaviconLinkForIcon(
              Services.io.newURI(tab.icon)
            ).spec;
          }
        }

        let result = new lazy.UrlbarResult(
          UrlbarUtils.RESULT_TYPE.REMOTE_TAB,
          UrlbarUtils.RESULT_SOURCE.TABS,
          ...lazy.UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
            url: [tab.url, UrlbarUtils.HIGHLIGHT.TYPED],
            title: [tab.title, UrlbarUtils.HIGHLIGHT.TYPED],
            device: client.name,
            icon: lazy.showRemoteIconsPref ? tab.icon : "",
            lastUsed: (tab.lastUsed || 0) * 1000,
          })
        );

        // We want to return the most relevant remote tabs and thus the most
        // recent ones. While SyncedTabs.jsm returns tabs that are sorted by
        // most recent client, then most recent tab, we can do better. For
        // example, the most recent client might have one recent tab and then
        // many very stale tabs. Those very stale tabs will push out more recent
        // tabs from staler clients. This provider first returns tabs from the
        // last 72 hours, sorted by client recency. Then, it adds remaining
        // tabs. We are not concerned about filling the remote tabs group with
        // stale tabs, because the muxer ensures remote tabs flex with other
        // results. It will only show the stale tabs if it has nothing else
        // to show.
        if (
          tab.lastUsed <=
          (Date.now() - RECENT_REMOTE_TAB_THRESHOLD_MS) / 1000
        ) {
          staleTabs.push(result);
        } else {
          addCallback(this, result);
          resultsAdded++;
        }
      }

      if (resultsAdded == queryContext.maxResults) {
        break;
      }
    }

    while (staleTabs.length && resultsAdded < queryContext.maxResults) {
      addCallback(this, staleTabs.shift());
      resultsAdded++;
    }
  }

  /**
   * Build the in-memory structure we use.
   */
  async buildItems() {
    // This is sorted by most recent client, most recent tab.
    let tabsData = [];
    // If Sync isn't initialized (either due to lag at startup or due to no user
    // being signed in), don't reach in to Weave.Service as that may initialize
    // Sync unnecessarily - we'll get an observer notification later when it
    // becomes ready and has synced a list of tabs.
    if (lazy.weaveXPCService.ready) {
      let clients = await lazy.SyncedTabs.getTabClients();
      lazy.SyncedTabs.sortTabClientsByLastUsed(clients);
      for (let client of clients) {
        for (let tab of client.tabs) {
          tabsData.push({ tab, client });
        }
      }
    }
    return tabsData;
  }

  /**
   * Ensure the cache is good.
   **/
  async ensureCache() {
    if (!_cache) {
      _cache = await this.buildItems();
    }
    return _cache;
  }

  observe(subject, topic, data) {
    switch (topic) {
      case "weave:engine:sync:finish":
        if (data == "tabs") {
          // The tabs engine just finished syncing, so may have a different list
          // of tabs then we previously cached.
          _cache = null;
        }
        break;

      case "weave:service:start-over":
        // Sync is being reset due to the user disconnecting - we must invalidate
        // the cache so we don't supply tabs from a different user.
        _cache = null;
        break;

      default:
        break;
    }
  }
}

export var UrlbarProviderRemoteTabs = new ProviderRemoteTabs();
