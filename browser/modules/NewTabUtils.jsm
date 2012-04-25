/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let EXPORTED_SYMBOLS = ["NewTabUtils"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gPrivateBrowsing",
  "@mozilla.org/privatebrowsing;1", "nsIPrivateBrowsingService");

XPCOMUtils.defineLazyModuleGetter(this, "Dict", "resource://gre/modules/Dict.jsm");

// The preference that tells whether this feature is enabled.
const PREF_NEWTAB_ENABLED = "browser.newtabpage.enabled";

// The maximum number of results we want to retrieve from history.
const HISTORY_RESULTS_LIMIT = 100;

// The gather telemetry topic.
const TOPIC_GATHER_TELEMETRY = "gather-telemetry";

/**
 * Singleton that provides storage functionality.
 */
let Storage = {
  /**
   * The dom storage instance used to persist data belonging to the New Tab Page.
   */
  get domStorage() {
    let uri = Services.io.newURI("about:newtab", null, null);
    let principal = Services.scriptSecurityManager.getCodebasePrincipal(uri);

    let sm = Services.domStorageManager;
    let storage = sm.getLocalStorageForPrincipal(principal, "");

    // Cache this value, overwrite the getter.
    let descriptor = {value: storage, enumerable: true};
    Object.defineProperty(this, "domStorage", descriptor);

    return storage;
  },

  /**
   * The current storage used to persist New Tab Page data. If we're currently
   * in private browsing mode this will return a PrivateBrowsingStorage
   * instance.
   */
  get currentStorage() {
    let storage = this.domStorage;

    // Check if we're starting in private browsing mode.
    if (gPrivateBrowsing.privateBrowsingEnabled)
      storage = new PrivateBrowsingStorage(storage);

    // Register an observer to listen for private browsing mode changes.
    Services.obs.addObserver(this, "private-browsing", true);

    // Cache this value, overwrite the getter.
    let descriptor = {value: storage, enumerable: true, writable: true};
    Object.defineProperty(this, "currentStorage", descriptor);

    return storage;
  },

  /**
   * Gets the value for a given key from the storage.
   * @param aKey The storage key (a string).
   * @param aDefault A default value if the key doesn't exist.
   * @return The value for the given key.
   */
  get: function Storage_get(aKey, aDefault) {
    let value;

    try {
      value = JSON.parse(this.currentStorage.getItem(aKey));
    } catch (e) {}

    return value || aDefault;
  },

  /**
   * Sets the storage value for a given key.
   * @param aKey The storage key (a string).
   * @param aValue The value to set.
   */
  set: function Storage_set(aKey, aValue) {
    this.currentStorage.setItem(aKey, JSON.stringify(aValue));
  },

  /**
   * Clears the storage and removes all values.
   */
  clear: function Storage_clear() {
    this.currentStorage.clear();
  },

  /**
   * Implements the nsIObserver interface to get notified about private
   * browsing mode changes.
   */
  observe: function Storage_observe(aSubject, aTopic, aData) {
    if (aData == "enter") {
      // When switching to private browsing mode we keep the current state
      // of the grid and provide a volatile storage for it that is
      // discarded upon leaving private browsing.
      this.currentStorage = new PrivateBrowsingStorage(this.domStorage);
    } else {
      // Reset to normal DOM storage.
      this.currentStorage = this.domStorage;

      // When switching back from private browsing we need to reset the
      // grid and re-read its values from the underlying storage. We don't
      // want any data from private browsing to show up.
      PinnedLinks.resetCache();
      BlockedLinks.resetCache();
    }
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference])
};

/**
 * This class implements a temporary storage used while the user is in private
 * browsing mode. It is discarded when leaving pb mode.
 */
function PrivateBrowsingStorage(aStorage) {
  this._data = new Dict();

  for (let i = 0; i < aStorage.length; i++) {
    let key = aStorage.key(i);
    this._data.set(key, aStorage.getItem(key));
  }
}

PrivateBrowsingStorage.prototype = {
  /**
   * The data store.
   */
  _data: null,

  /**
   * Gets the value for a given key from the storage.
   * @param aKey The storage key.
   * @param aDefault A default value if the key doesn't exist.
   * @return The value for the given key.
   */
  getItem: function PrivateBrowsingStorage_getItem(aKey) {
    return this._data.get(aKey);
  },

  /**
   * Sets the storage value for a given key.
   * @param aKey The storage key.
   * @param aValue The value to set.
   */
  setItem: function PrivateBrowsingStorage_setItem(aKey, aValue) {
    this._data.set(aKey, aValue);
  },

  /**
   * Clears the storage and removes all values.
   */
  clear: function PrivateBrowsingStorage_clear() {
    this._data.listkeys().forEach(function (aKey) {
      this._data.del(aKey);
    }, this);
  }
};

/**
 * Singleton that serves as a registry for all open 'New Tab Page's.
 */
let AllPages = {
  /**
   * The array containing all active pages.
   */
  _pages: [],

  /**
   * Cached value that tells whether the New Tab Page feature is enabled.
   */
  _enabled: null,

  /**
   * Adds a page to the internal list of pages.
   * @param aPage The page to register.
   */
  register: function AllPages_register(aPage) {
    this._pages.push(aPage);
    this._addObserver();
  },

  /**
   * Removes a page from the internal list of pages.
   * @param aPage The page to unregister.
   */
  unregister: function AllPages_unregister(aPage) {
    let index = this._pages.indexOf(aPage);
    this._pages.splice(index, 1);
  },

  /**
   * Returns whether the 'New Tab Page' is enabled.
   */
  get enabled() {
    if (this._enabled === null)
      this._enabled = Services.prefs.getBoolPref(PREF_NEWTAB_ENABLED);

    return this._enabled;
  },

  /**
   * Enables or disables the 'New Tab Page' feature.
   */
  set enabled(aEnabled) {
    if (this.enabled != aEnabled)
      Services.prefs.setBoolPref(PREF_NEWTAB_ENABLED, !!aEnabled);
  },

  /**
   * Returns the number of registered New Tab Pages (i.e. the number of open
   * about:newtab instances).
   */
  get length() {
    return this._pages.length;
  },

  /**
   * Updates all currently active pages but the given one.
   * @param aExceptPage The page to exclude from updating.
   */
  update: function AllPages_update(aExceptPage) {
    this._pages.forEach(function (aPage) {
      if (aExceptPage != aPage)
        aPage.update();
    });
  },

  /**
   * Implements the nsIObserver interface to get notified when the preference
   * value changes.
   */
  observe: function AllPages_observe() {
    // Clear the cached value.
    this._enabled = null;

    let args = Array.slice(arguments);

    this._pages.forEach(function (aPage) {
      aPage.observe.apply(aPage, args);
    }, this);
  },

  /**
   * Adds a preference observer and turns itself into a no-op after the first
   * invokation.
   */
  _addObserver: function AllPages_addObserver() {
    Services.prefs.addObserver(PREF_NEWTAB_ENABLED, this, true);
    this._addObserver = function () {};
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference])
};

/**
 * Singleton that keeps track of all pinned links and their positions in the
 * grid.
 */
let PinnedLinks = {
  /**
   * The cached list of pinned links.
   */
  _links: null,

  /**
   * The array of pinned links.
   */
  get links() {
    if (!this._links)
      this._links = Storage.get("pinnedLinks", []);

    return this._links;
  },

  /**
   * Pins a link at the given position.
   * @param aLink The link to pin.
   * @param aIndex The grid index to pin the cell at.
   */
  pin: function PinnedLinks_pin(aLink, aIndex) {
    // Clear the link's old position, if any.
    this.unpin(aLink);

    this.links[aIndex] = aLink;
    Storage.set("pinnedLinks", this.links);
  },

  /**
   * Unpins a given link.
   * @param aLink The link to unpin.
   */
  unpin: function PinnedLinks_unpin(aLink) {
    let index = this._indexOfLink(aLink);
    if (index != -1) {
      this.links[index] = null;
      Storage.set("pinnedLinks", this.links);
    }
  },

  /**
   * Checks whether a given link is pinned.
   * @params aLink The link to check.
   * @return whether The link is pinned.
   */
  isPinned: function PinnedLinks_isPinned(aLink) {
    return this._indexOfLink(aLink) != -1;
  },

  /**
   * Resets the links cache.
   */
  resetCache: function PinnedLinks_resetCache() {
    this._links = null;
  },

  /**
   * Finds the index of a given link in the list of pinned links.
   * @param aLink The link to find an index for.
   * @return The link's index.
   */
  _indexOfLink: function PinnedLinks_indexOfLink(aLink) {
    for (let i = 0; i < this.links.length; i++) {
      let link = this.links[i];
      if (link && link.url == aLink.url)
        return i;
    }

    // The given link is unpinned.
    return -1;
  }
};

/**
 * Singleton that keeps track of all blocked links in the grid.
 */
let BlockedLinks = {
  /**
   * The cached list of blocked links.
   */
  _links: null,

  /**
   * The list of blocked links.
   */
  get links() {
    if (!this._links)
      this._links = Storage.get("blockedLinks", {});

    return this._links;
  },

  /**
   * Blocks a given link.
   * @param aLink The link to block.
   */
  block: function BlockedLinks_block(aLink) {
    this.links[aLink.url] = 1;

    // Make sure we unpin blocked links.
    PinnedLinks.unpin(aLink);

    Storage.set("blockedLinks", this.links);
  },

  /**
   * Unblocks a given link.
   * @param aLink The link to unblock.
   */
  unblock: function BlockedLinks_unblock(aLink) {
    if (this.isBlocked(aLink))
      delete this.links[aLink.url];
  },

  /**
   * Returns whether a given link is blocked.
   * @param aLink The link to check.
   */
  isBlocked: function BlockedLinks_isBlocked(aLink) {
    return (aLink.url in this.links);
  },

  /**
   * Checks whether the list of blocked links is empty.
   * @return Whether the list is empty.
   */
  isEmpty: function BlockedLinks_isEmpty() {
    return Object.keys(this.links).length == 0;
  },

  /**
   * Resets the links cache.
   */
  resetCache: function BlockedLinks_resetCache() {
    this._links = null;
  }
};

/**
 * Singleton that serves as the default link provider for the grid. It queries
 * the history to retrieve the most frequently visited sites.
 */
let PlacesProvider = {
  /**
   * Gets the current set of links delivered by this provider.
   * @param aCallback The function that the array of links is passed to.
   */
  getLinks: function PlacesProvider_getLinks(aCallback) {
    let options = PlacesUtils.history.getNewQueryOptions();
    options.maxResults = HISTORY_RESULTS_LIMIT;

    // Sort by frecency, descending.
    options.sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_FRECENCY_DESCENDING

    let links = [];

    let callback = {
      handleResult: function (aResultSet) {
        let row;

        while (row = aResultSet.getNextRow()) {
          let url = row.getResultByIndex(1);
          let title = row.getResultByIndex(2);
          links.push({url: url, title: title});
        }
      },

      handleError: function (aError) {
        // Should we somehow handle this error?
        aCallback([]);
      },

      handleCompletion: function (aReason) {
        aCallback(links);
      }
    };

    // Execute the query.
    let query = PlacesUtils.history.getNewQuery();
    let db = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase);
    db.asyncExecuteLegacyQueries([query], 1, options, callback);
  }
};

/**
 * Singleton that provides access to all links contained in the grid (including
 * the ones that don't fit on the grid). A link is a plain object with title
 * and url properties.
 *
 * Example:
 *
 * {url: "http://www.mozilla.org/", title: "Mozilla"}
 */
let Links = {
  /**
   * The links cache.
   */
  _links: null,

  /**
   * The default provider for links.
   */
  _provider: PlacesProvider,

  /**
   * List of callbacks waiting for the cache to be populated.
   */
  _populateCallbacks: [],

  /**
   * Populates the cache with fresh links from the current provider.
   * @param aCallback The callback to call when finished (optional).
   * @param aForce When true, populates the cache even when it's already filled.
   */
  populateCache: function Links_populateCache(aCallback, aForce) {
    let callbacks = this._populateCallbacks;

    // Enqueue the current callback.
    callbacks.push(aCallback);

    // There was a callback waiting already, thus the cache has not yet been
    // populated.
    if (callbacks.length > 1)
      return;

    function executeCallbacks() {
      while (callbacks.length) {
        let callback = callbacks.shift();
        if (callback) {
          try {
            callback();
          } catch (e) {
            // We want to proceed even if a callback fails.
          }
        }
      }
    }

    if (this._links && !aForce) {
      executeCallbacks();
    } else {
      this._provider.getLinks(function (aLinks) {
        this._links = aLinks;
        executeCallbacks();
      }.bind(this));

      this._addObserver();
    }
  },

  /**
   * Gets the current set of links contained in the grid.
   * @return The links in the grid.
   */
  getLinks: function Links_getLinks() {
    let pinnedLinks = Array.slice(PinnedLinks.links);

    // Filter blocked and pinned links.
    let links = this._links.filter(function (link) {
      return !BlockedLinks.isBlocked(link) && !PinnedLinks.isPinned(link);
    });

    // Try to fill the gaps between pinned links.
    for (let i = 0; i < pinnedLinks.length && links.length; i++)
      if (!pinnedLinks[i])
        pinnedLinks[i] = links.shift();

    // Append the remaining links if any.
    if (links.length)
      pinnedLinks = pinnedLinks.concat(links);

    return pinnedLinks;
  },

  /**
   * Resets the links cache.
   */
  resetCache: function Links_resetCache() {
    this._links = null;
  },

  /**
   * Implements the nsIObserver interface to get notified about browser history
   * sanitization.
   */
  observe: function Links_observe(aSubject, aTopic, aData) {
    // Make sure to update open about:newtab instances. If there are no opened
    // pages we can just wait for the next new tab to populate the cache again.
    if (AllPages.length && AllPages.enabled)
      this.populateCache(function () { AllPages.update() }, true);
    else
      this._links = null;
  },

  /**
   * Adds a sanitization observer and turns itself into a no-op after the first
   * invokation.
   */
  _addObserver: function Links_addObserver() {
    Services.obs.addObserver(this, "browser:purge-session-history", true);
    this._addObserver = function () {};
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference])
};

/**
 * Singleton used to collect telemetry data.
 *
 */
let Telemetry = {
  /**
   * Initializes object.
   */
  init: function Telemetry_init() {
    Services.obs.addObserver(this, TOPIC_GATHER_TELEMETRY, false);
  },

  /**
   * Collects data.
   */
  _collect: function Telemetry_collect() {
    let probes = [
      { histogram: "NEWTAB_PAGE_ENABLED",
        value: AllPages.enabled },
      { histogram: "NEWTAB_PAGE_PINNED_SITES_COUNT",
        value: PinnedLinks.links.length },
      { histogram: "NEWTAB_PAGE_BLOCKED_SITES_COUNT",
        value: Object.keys(BlockedLinks.links).length }
    ];

    probes.forEach(function Telemetry_collect_forEach(aProbe) {
      Services.telemetry.getHistogramById(aProbe.histogram)
        .add(aProbe.value);
    });
  },

  /**
   * Listens for gather telemetry topic.
   */
  observe: function Telemetry_observe(aSubject, aTopic, aData) {
    this._collect();
  }
};

Telemetry.init();

/**
 * Singleton that provides the public API of this JSM.
 */
let NewTabUtils = {
  /**
   * Restores all sites that have been removed from the grid.
   */
  restore: function NewTabUtils_restore() {
    Storage.clear();
    Links.resetCache();
    PinnedLinks.resetCache();
    BlockedLinks.resetCache();

    Links.populateCache(function () {
      AllPages.update();
    }, true);
  },

  allPages: AllPages,
  links: Links,
  pinnedLinks: PinnedLinks,
  blockedLinks: BlockedLinks
};
