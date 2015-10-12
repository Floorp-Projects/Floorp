/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["RemoteNewTabUtils"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PageThumbs",
  "resource://gre/modules/PageThumbs.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "BinarySearch",
  "resource://gre/modules/BinarySearch.jsm");

XPCOMUtils.defineLazyGetter(this, "gPrincipal", function () {
  let uri = Services.io.newURI("about:newtab", null, null);
  return Services.scriptSecurityManager.createCodebasePrincipal(uri, {});
});

// The maximum number of results PlacesProvider retrieves from history.
const HISTORY_RESULTS_LIMIT = 100;

// The maximum number of links Links.getLinks will return.
const LINKS_GET_LINKS_LIMIT = 100;

/**
 * Singleton that serves as the default link provider for the grid. It queries
 * the history to retrieve the most frequently visited sites.
 */
let PlacesProvider = {
  /**
   * A count of how many batch updates are under way (batches may be nested, so
   * we keep a counter instead of a simple bool).
   **/
  _batchProcessingDepth: 0,

  /**
   * A flag that tracks whether onFrecencyChanged was notified while a batch
   * operation was in progress, to tell us whether to take special action after
   * the batch operation completes.
   **/
  _batchCalledFrecencyChanged: false,

  /**
   * Set this to change the maximum number of links the provider will provide.
   */
  maxNumLinks: HISTORY_RESULTS_LIMIT,

  /**
   * Must be called before the provider is used.
   */
  init: function PlacesProvider_init() {
    PlacesUtils.history.addObserver(this, true);
  },

  /**
   * Gets the current set of links delivered by this provider.
   * @param aCallback The function that the array of links is passed to.
   */
  getLinks: function PlacesProvider_getLinks(aCallback) {
    let options = PlacesUtils.history.getNewQueryOptions();
    options.maxResults = this.maxNumLinks;

    // Sort by frecency, descending.
    options.sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_FRECENCY_DESCENDING

    let links = [];

    let callback = {
      handleResult: function (aResultSet) {
        let row;

        while ((row = aResultSet.getNextRow())) {
          let url = row.getResultByIndex(1);
          if (LinkChecker.checkLoadURI(url)) {
            let title = row.getResultByIndex(2);
            let frecency = row.getResultByIndex(12);
            let lastVisitDate = row.getResultByIndex(5);
            links.push({
              url: url,
              title: title,
              frecency: frecency,
              lastVisitDate: lastVisitDate,
              type: "history",
            });
          }
        }
      },

      handleError: function (aError) {
        // Should we somehow handle this error?
        aCallback([]);
      },

      handleCompletion: function (aReason) {
        // The Places query breaks ties in frecency by place ID descending, but
        // that's different from how Links.compareLinks breaks ties, because
        // compareLinks doesn't have access to place IDs.  It's very important
        // that the initial list of links is sorted in the same order imposed by
        // compareLinks, because Links uses compareLinks to perform binary
        // searches on the list.  So, ensure the list is so ordered.
        let i = 1;
        let outOfOrder = [];
        while (i < links.length) {
          if (Links.compareLinks(links[i - 1], links[i]) > 0)
            outOfOrder.push(links.splice(i, 1)[0]);
          else
            i++;
        }
        for (let link of outOfOrder) {
          i = BinarySearch.insertionIndexOf(Links.compareLinks, links, link);
          links.splice(i, 0, link);
        }

        aCallback(links);
      }
    };

    // Execute the query.
    let query = PlacesUtils.history.getNewQuery();
    let db = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase);
    db.asyncExecuteLegacyQueries([query], 1, options, callback);
  },

  /**
   * Registers an object that will be notified when the provider's links change.
   * @param aObserver An object with the following optional properties:
   *        * onLinkChanged: A function that's called when a single link
   *          changes.  It's passed the provider and the link object.  Only the
   *          link's `url` property is guaranteed to be present.  If its `title`
   *          property is present, then its title has changed, and the
   *          property's value is the new title.  If any sort properties are
   *          present, then its position within the provider's list of links may
   *          have changed, and the properties' values are the new sort-related
   *          values.  Note that this link may not necessarily have been present
   *          in the lists returned from any previous calls to getLinks.
   *        * onManyLinksChanged: A function that's called when many links
   *          change at once.  It's passed the provider.  You should call
   *          getLinks to get the provider's new list of links.
   */
  addObserver: function PlacesProvider_addObserver(aObserver) {
    this._observers.push(aObserver);
  },

  _observers: [],

  /**
   * Called by the history service.
   */
  onBeginUpdateBatch: function() {
    this._batchProcessingDepth += 1;
  },

  onEndUpdateBatch: function() {
    this._batchProcessingDepth -= 1;
    if (this._batchProcessingDepth == 0 && this._batchCalledFrecencyChanged) {
      this.onManyFrecenciesChanged();
      this._batchCalledFrecencyChanged = false;
    }
  },

  onDeleteURI: function PlacesProvider_onDeleteURI(aURI, aGUID, aReason) {
    // let observers remove sensetive data associated with deleted visit
    this._callObservers("onDeleteURI", {
      url: aURI.spec,
    });
  },

  onClearHistory: function() {
    this._callObservers("onClearHistory")
  },

  /**
   * Called by the history service.
   */
  onFrecencyChanged: function PlacesProvider_onFrecencyChanged(aURI, aNewFrecency, aGUID, aHidden, aLastVisitDate) {
    // If something is doing a batch update of history entries we don't want
    // to do lots of work for each record. So we just track the fact we need
    // to call onManyFrecenciesChanged() once the batch is complete.
    if (this._batchProcessingDepth > 0) {
      this._batchCalledFrecencyChanged = true;
      return;
    }
    // The implementation of the query in getLinks excludes hidden and
    // unvisited pages, so it's important to exclude them here, too.
    if (!aHidden && aLastVisitDate) {
      this._callObservers("onLinkChanged", {
        url: aURI.spec,
        frecency: aNewFrecency,
        lastVisitDate: aLastVisitDate,
        type: "history",
      });
    }
  },

  /**
   * Called by the history service.
   */
  onManyFrecenciesChanged: function PlacesProvider_onManyFrecenciesChanged() {
    this._callObservers("onManyLinksChanged");
  },

  /**
   * Called by the history service.
   */
  onTitleChanged: function PlacesProvider_onTitleChanged(aURI, aNewTitle, aGUID) {
    this._callObservers("onLinkChanged", {
      url: aURI.spec,
      title: aNewTitle
    });
  },

  _callObservers: function PlacesProvider__callObservers(aMethodName, aArg) {
    for (let obs of this._observers) {
      if (obs[aMethodName]) {
        try {
          obs[aMethodName](this, aArg);
        } catch (err) {
          Cu.reportError(err);
        }
      }
    }
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver,
                                         Ci.nsISupportsWeakReference]),
};

/**
 * Singleton that provides access to all links contained in the grid (including
 * the ones that don't fit on the grid). A link is a plain object that looks
 * like this:
 *
 * {
 *   url: "http://www.mozilla.org/",
 *   title: "Mozilla",
 *   frecency: 1337,
 *   lastVisitDate: 1394678824766431,
 * }
 */
let Links = {
  /**
   * The maximum number of links returned by getLinks.
   */
  maxNumLinks: LINKS_GET_LINKS_LIMIT,

  /**
   * A mapping from each provider to an object { sortedLinks, siteMap, linkMap }.
   * sortedLinks is the cached, sorted array of links for the provider.
   * siteMap is a mapping from base domains to URL count associated with the domain.
   *         siteMap is used to look up a user's top sites that can be targeted
   *         with a suggested tile.
   * linkMap is a Map from link URLs to link objects.
   */
  _providers: new Map(),

  /**
   * The properties of link objects used to sort them.
   */
  _sortProperties: [
    "frecency",
    "lastVisitDate",
    "url",
  ],

  /**
   * List of callbacks waiting for the cache to be populated.
   */
  _populateCallbacks: [],

  /**
   * A list of objects that are observing links updates.
   */
  _observers: [],

  /**
   * Registers an object that will be notified when links updates.
   */
  addObserver: function (aObserver) {
    this._observers.push(aObserver);
  },

  /**
   * Adds a link provider.
   * @param aProvider The link provider.
   */
  addProvider: function Links_addProvider(aProvider) {
    this._providers.set(aProvider, null);
    aProvider.addObserver(this);
  },

  /**
   * Removes a link provider.
   * @param aProvider The link provider.
   */
  removeProvider: function Links_removeProvider(aProvider) {
    if (!this._providers.delete(aProvider))
      throw new Error("Unknown provider");
  },

  /**
   * Populates the cache with fresh links from the providers.
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

    let numProvidersRemaining = this._providers.size;
    for (let [provider, links] of this._providers) {
      this._populateProviderCache(provider, () => {
        if (--numProvidersRemaining == 0)
          executeCallbacks();
      }, aForce);
    }
  },

  /**
   * Gets the current set of links contained in the grid.
   * @return The links in the grid.
   */
  getLinks: function Links_getLinks() {
    let links = this._getMergedProviderLinks();

    let sites = new Set();

    // Filter duplicate base domains.
    links = links.filter(function (link) {
      let site = RemoteNewTabUtils.extractSite(link.url);
      link.baseDomain = site;
      if (site == null || sites.has(site))
        return false;
      sites.add(site);

      return true;
    });

    return links;
  },

  /**
   * Resets the links cache.
   */
  resetCache: function Links_resetCache() {
    for (let provider of this._providers.keys()) {
      this._providers.set(provider, null);
    }
  },

  /**
   * Compares two links.
   * @param aLink1 The first link.
   * @param aLink2 The second link.
   * @return A negative number if aLink1 is ordered before aLink2, zero if
   *         aLink1 and aLink2 have the same ordering, or a positive number if
   *         aLink1 is ordered after aLink2.
   *
   * @note compareLinks's this object is bound to Links below.
   */
  compareLinks: function Links_compareLinks(aLink1, aLink2) {
    for (let prop of this._sortProperties) {
      if (!(prop in aLink1) || !(prop in aLink2))
        throw new Error("Comparable link missing required property: " + prop);
    }
    return aLink2.frecency - aLink1.frecency ||
           aLink2.lastVisitDate - aLink1.lastVisitDate ||
           aLink1.url.localeCompare(aLink2.url);
  },

  _incrementSiteMap: function(map, link) {
    let site = RemoteNewTabUtils.extractSite(link.url);
    map.set(site, (map.get(site) || 0) + 1);
  },

  _decrementSiteMap: function(map, link) {
    let site = RemoteNewTabUtils.extractSite(link.url);
    let previousURLCount = map.get(site);
    if (previousURLCount === 1) {
      map.delete(site);
    } else {
      map.set(site, previousURLCount - 1);
    }
  },

  /**
    * Update the siteMap cache based on the link given and whether we need
    * to increment or decrement it. We do this by iterating over all stored providers
    * to find which provider this link already exists in. For providers that
    * have this link, we will adjust siteMap for them accordingly.
    *
    * @param aLink The link that will affect siteMap
    * @param increment A boolean for whether to increment or decrement siteMap
    */
  _adjustSiteMapAndNotify: function(aLink, increment=true) {
    for (let [provider, cache] of this._providers) {
      // We only update siteMap if aLink is already stored in linkMap.
      if (cache.linkMap.get(aLink.url)) {
        if (increment) {
          this._incrementSiteMap(cache.siteMap, aLink);
          continue;
        }
        this._decrementSiteMap(cache.siteMap, aLink);
      }
    }
    this._callObservers("onLinkChanged", aLink);
  },

  populateProviderCache: function(provider, callback) {
    if (!this._providers.has(provider)) {
      throw new Error("Can only populate provider cache for existing provider.");
    }

    return this._populateProviderCache(provider, callback, false);
  },

  /**
   * Calls getLinks on the given provider and populates our cache for it.
   * @param aProvider The provider whose cache will be populated.
   * @param aCallback The callback to call when finished.
   * @param aForce When true, populates the provider's cache even when it's
   *               already filled.
   */
  _populateProviderCache: function (aProvider, aCallback, aForce) {
    let cache = this._providers.get(aProvider);
    let createCache = !cache;
    if (createCache) {
      cache = {
        // Start with a resolved promise.
        populatePromise: new Promise(resolve => resolve()),
      };
      this._providers.set(aProvider, cache);
    }
    // Chain the populatePromise so that calls are effectively queued.
    cache.populatePromise = cache.populatePromise.then(() => {
      return new Promise(resolve => {
        if (!createCache && !aForce) {
          aCallback();
          resolve();
          return;
        }
        aProvider.getLinks(links => {
          // Filter out null and undefined links so we don't have to deal with
          // them in getLinks when merging links from providers.
          links = links.filter((link) => !!link);
          cache.sortedLinks = links;
          cache.siteMap = links.reduce((map, link) => {
            this._incrementSiteMap(map, link);
            return map;
          }, new Map());
          cache.linkMap = links.reduce((map, link) => {
            map.set(link.url, link);
            return map;
          }, new Map());
          aCallback();
          resolve();
        });
      });
    });
  },

  /**
   * Merges the cached lists of links from all providers whose lists are cached.
   * @return The merged list.
   */
  _getMergedProviderLinks: function Links__getMergedProviderLinks() {
    // Build a list containing a copy of each provider's sortedLinks list.
    let linkLists = [];
    for (let provider of this._providers.keys()) {
      let links = this._providers.get(provider);
      if (links && links.sortedLinks) {
        linkLists.push(links.sortedLinks.slice());
      }
    }

    function getNextLink() {
      let minLinks = null;
      for (let links of linkLists) {
        if (links.length &&
            (!minLinks || Links.compareLinks(links[0], minLinks[0]) < 0))
          minLinks = links;
      }
      return minLinks ? minLinks.shift() : null;
    }

    let finalLinks = [];
    for (let nextLink = getNextLink();
         nextLink && finalLinks.length < this.maxNumLinks;
         nextLink = getNextLink()) {
      finalLinks.push(nextLink);
    }

    return finalLinks;
  },

  /**
   * Called by a provider to notify us when a single link changes.
   * @param aProvider The provider whose link changed.
   * @param aLink The link that changed.  If the link is new, it must have all
   *              of the _sortProperties.  Otherwise, it may have as few or as
   *              many as is convenient.
   * @param aIndex The current index of the changed link in the sortedLinks
                   cache in _providers. Defaults to -1 if the provider doesn't know the index
   * @param aDeleted Boolean indicating if the provider has deleted the link.
   */
  onLinkChanged: function Links_onLinkChanged(aProvider, aLink, aIndex=-1, aDeleted=false) {
    if (!("url" in aLink))
      throw new Error("Changed links must have a url property");

    let links = this._providers.get(aProvider);
    if (!links)
      // This is not an error, it just means that between the time the provider
      // was added and the future time we call getLinks on it, it notified us of
      // a change.
      return;

    let { sortedLinks, siteMap, linkMap } = links;
    let existingLink = linkMap.get(aLink.url);
    let insertionLink = null;

    if (existingLink) {
      // Update our copy's position in O(lg n) by first removing it from its
      // list.  It's important to do this before modifying its properties.
      if (this._sortProperties.some(prop => prop in aLink)) {
        let idx = aIndex;
        if (idx < 0) {
          idx = this._indexOf(sortedLinks, existingLink);
        } else if (this.compareLinks(aLink, sortedLinks[idx]) != 0) {
          throw new Error("aLink should be the same as sortedLinks[idx]");
        }

        if (idx < 0) {
          throw new Error("Link should be in _sortedLinks if in _linkMap");
        }
        sortedLinks.splice(idx, 1);

        if (aDeleted) {
          linkMap.delete(existingLink.url);
          this._decrementSiteMap(siteMap, existingLink);
        } else {
          // Update our copy's properties.
          Object.assign(existingLink, aLink);

          // Finally, reinsert our copy below.
          insertionLink = existingLink;
        }
      }
      // Update our copy's title in O(1).
      if ("title" in aLink && aLink.title != existingLink.title) {
        existingLink.title = aLink.title;
      }
    }
    else if (this._sortProperties.every(prop => prop in aLink)) {
      // Before doing the O(lg n) insertion below, do an O(1) check for the
      // common case where the new link is too low-ranked to be in the list.
      if (sortedLinks.length && sortedLinks.length == aProvider.maxNumLinks) {
        let lastLink = sortedLinks[sortedLinks.length - 1];
        if (this.compareLinks(lastLink, aLink) < 0) {
          return;
        }
      }
      // Copy the link object so that changes later made to it by the caller
      // don't affect our copy.
      insertionLink = {};
      for (let prop in aLink) {
        insertionLink[prop] = aLink[prop];
      }
      linkMap.set(aLink.url, insertionLink);
      this._incrementSiteMap(siteMap, aLink);
    }

    if (insertionLink) {
      let idx = this._insertionIndexOf(sortedLinks, insertionLink);
      sortedLinks.splice(idx, 0, insertionLink);
      if (sortedLinks.length > aProvider.maxNumLinks) {
        let lastLink = sortedLinks.pop();
        linkMap.delete(lastLink.url);
        this._decrementSiteMap(siteMap, lastLink);
      }
    }
  },

  /**
   * Called by a provider to notify us when many links change.
   */
  onManyLinksChanged: function Links_onManyLinksChanged(aProvider) {
    this._populateProviderCache(aProvider, () => {}, true);
  },

  _indexOf: function Links__indexOf(aArray, aLink) {
    return this._binsearch(aArray, aLink, "indexOf");
  },

  _insertionIndexOf: function Links__insertionIndexOf(aArray, aLink) {
    return this._binsearch(aArray, aLink, "insertionIndexOf");
  },

  _binsearch: function Links__binsearch(aArray, aLink, aMethod) {
    return BinarySearch[aMethod](this.compareLinks, aArray, aLink);
  },

  _callObservers(methodName, ...args) {
    for (let obs of this._observers) {
      if (typeof(obs[methodName]) == "function") {
        try {
          obs[methodName](this, ...args);
        } catch (err) {
          Cu.reportError(err);
        }
      }
    }
  },
};

Links.compareLinks = Links.compareLinks.bind(Links);

/**
 * Singleton that checks if a given link should be displayed on about:newtab
 * or if we should rather not do it for security reasons. URIs that inherit
 * their caller's principal will be filtered.
 */
let LinkChecker = {
  _cache: {},

  get flags() {
    return Ci.nsIScriptSecurityManager.DISALLOW_INHERIT_PRINCIPAL |
           Ci.nsIScriptSecurityManager.DONT_REPORT_ERRORS;
  },

  checkLoadURI: function LinkChecker_checkLoadURI(aURI) {
    if (!(aURI in this._cache))
      this._cache[aURI] = this._doCheckLoadURI(aURI);

    return this._cache[aURI];
  },

  _doCheckLoadURI: function Links_doCheckLoadURI(aURI) {
    try {
      Services.scriptSecurityManager.
        checkLoadURIStrWithPrincipal(gPrincipal, aURI, this.flags);
      return true;
    } catch (e) {
      // We got a weird URI or one that would inherit the caller's principal.
      return false;
    }
  }
};

let ExpirationFilter = {
  init: function ExpirationFilter_init() {
    PageThumbs.addExpirationFilter(this);
  },

  filterForThumbnailExpiration:
  function ExpirationFilter_filterForThumbnailExpiration(aCallback) {
    Links.populateCache(function () {
      let urls = [];

      // Add all URLs to the list that we want to keep thumbnails for.
      for (let link of Links.getLinks().slice(0, 25)) {
        if (link && link.url)
          urls.push(link.url);
      }

      aCallback(urls);
    });
  }
};

/**
 * Singleton that provides the public API of this JSM.
 */
this.RemoteNewTabUtils = {
  _initialized: false,

  /**
   * Extract a "site" from a url in a way that multiple urls of a "site" returns
   * the same "site."
   * @param aUrl Url spec string
   * @return The "site" string or null
   */
  extractSite: function Links_extractSite(url) {
    let host;
    try {
      // Note that nsIURI.asciiHost throws NS_ERROR_FAILURE for some types of
      // URIs, including jar and moz-icon URIs.
      host = Services.io.newURI(url, null, null).asciiHost;
    } catch (ex) {
      return null;
    }

    // Strip off common subdomains of the same site (e.g., www, load balancer)
    return host.replace(/^(m|mobile|www\d*)\./, "");
  },

  init: function RemoteNewTabUtils_init() {
    if (this.initWithoutProviders()) {
      PlacesProvider.init();
      Links.addProvider(PlacesProvider);
    }
  },

  initWithoutProviders: function RemoteNewTabUtils_initWithoutProviders() {
    if (!this._initialized) {
      this._initialized = true;
      ExpirationFilter.init();
      return true;
    }
    return false;
  },

  getProviderLinks: function(aProvider) {
    let cache = Links._providers.get(aProvider);
    if (cache && cache.sortedLinks) {
      return cache.sortedLinks;
    }
    return [];
  },

  isTopSiteGivenProvider: function(aSite, aProvider) {
    let cache = Links._providers.get(aProvider);
    if (cache && cache.siteMap) {
      return cache.siteMap.has(aSite);
    }
    return false;
  },

  isTopPlacesSite: function(aSite) {
    return this.isTopSiteGivenProvider(aSite, PlacesProvider);
  },

  links: Links,
  linkChecker: LinkChecker,
  placesProvider: PlacesProvider
};
