/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["DirectoryLinksProvider"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const XMLHttpRequest =
  Components.Constructor("@mozilla.org/xmlextras/xmlhttprequest;1", "nsIXMLHttpRequest");

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Timer.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NewTabUtils",
  "resource://gre/modules/NewTabUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
  "resource://gre/modules/osfile.jsm")
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyGetter(this, "gTextDecoder", () => {
  return new TextDecoder();
});

// The filename where directory links are stored locally
const DIRECTORY_LINKS_FILE = "directoryLinks.json";
const DIRECTORY_LINKS_TYPE = "application/json";

// The preference that tells whether to match the OS locale
const PREF_MATCH_OS_LOCALE = "intl.locale.matchOS";

// The preference that tells what locale the user selected
const PREF_SELECTED_LOCALE = "general.useragent.locale";

// The preference that tells where to obtain directory links
const PREF_DIRECTORY_SOURCE = "browser.newtabpage.directory.source";

// The preference that tells where to send click/view pings
const PREF_DIRECTORY_PING = "browser.newtabpage.directory.ping";

// The preference that tells if newtab is enhanced
const PREF_NEWTAB_ENHANCED = "browser.newtabpage.enhanced";

// Only allow link urls that are http(s)
const ALLOWED_LINK_SCHEMES = new Set(["http", "https"]);

// Only allow link image urls that are https or data
const ALLOWED_IMAGE_SCHEMES = new Set(["https", "data"]);

// The frecency of a directory link
const DIRECTORY_FRECENCY = 1000;

// The frecency of a related link
const RELATED_FRECENCY = Infinity;

// Divide frecency by this amount for pings
const PING_SCORE_DIVISOR = 10000;

// Allowed ping actions remotely stored as columns: case-insensitive [a-z0-9_]
const PING_ACTIONS = ["block", "click", "pin", "sponsored", "sponsored_link", "unpin", "view"];

/**
 * Singleton that serves as the provider of directory links.
 * Directory links are a hard-coded set of links shown if a user's link
 * inventory is empty.
 */
let DirectoryLinksProvider = {

  __linksURL: null,

  _observers: new Set(),

  // links download deferred, resolved upon download completion
  _downloadDeferred: null,

  // download default interval is 24 hours in milliseconds
  _downloadIntervalMS: 86400000,

  /**
   * A mapping from eTLD+1 to an enhanced link objects
   */
  _enhancedLinks: new Map(),

  /**
   * A mapping from site to a list of related link objects
   */
  _relatedLinks: new Map(),

  /**
   * A set of top sites that we can provide related links for
   */
  _topSitesWithRelatedLinks: new Set(),

  get _observedPrefs() Object.freeze({
    enhanced: PREF_NEWTAB_ENHANCED,
    linksURL: PREF_DIRECTORY_SOURCE,
    matchOSLocale: PREF_MATCH_OS_LOCALE,
    prefSelectedLocale: PREF_SELECTED_LOCALE,
  }),

  get _linksURL() {
    if (!this.__linksURL) {
      try {
        this.__linksURL = Services.prefs.getCharPref(this._observedPrefs["linksURL"]);
      }
      catch (e) {
        Cu.reportError("Error fetching directory links url from prefs: " + e);
      }
    }
    return this.__linksURL;
  },

  /**
   * Gets the currently selected locale for display.
   * @return  the selected locale or "en-US" if none is selected
   */
  get locale() {
    let matchOS;
    try {
      matchOS = Services.prefs.getBoolPref(PREF_MATCH_OS_LOCALE);
    }
    catch (e) {}

    if (matchOS) {
      return Services.locale.getLocaleComponentForUserAgent();
    }

    try {
      let locale = Services.prefs.getComplexValue(PREF_SELECTED_LOCALE,
                                                  Ci.nsIPrefLocalizedString);
      if (locale) {
        return locale.data;
      }
    }
    catch (e) {}

    try {
      return Services.prefs.getCharPref(PREF_SELECTED_LOCALE);
    }
    catch (e) {}

    return "en-US";
  },

  /**
   * Set appropriate default ping behavior controlled by enhanced pref
   */
  _setDefaultEnhanced: function DirectoryLinksProvider_setDefaultEnhanced() {
    if (!Services.prefs.prefHasUserValue(PREF_NEWTAB_ENHANCED)) {
      let enhanced = true;
      try {
        // Default to not enhanced if DNT is set to tell websites to not track
        if (Services.prefs.getBoolPref("privacy.donottrackheader.enabled")) {
          enhanced = false;
        }
      }
      catch(ex) {}
      Services.prefs.setBoolPref(PREF_NEWTAB_ENHANCED, enhanced);
    }
  },

  observe: function DirectoryLinksProvider_observe(aSubject, aTopic, aData) {
    if (aTopic == "nsPref:changed") {
      switch (aData) {
        // Re-set the default in case the user clears the pref
        case this._observedPrefs.enhanced:
          this._setDefaultEnhanced();
          break;

        case this._observedPrefs.linksURL:
          delete this.__linksURL;
          // fallthrough

        // Force directory download on changes to fetch related prefs
        case this._observedPrefs.matchOSLocale:
        case this._observedPrefs.prefSelectedLocale:
          this._fetchAndCacheLinksIfNecessary(true);
          break;
      }
    }
  },

  _addPrefsObserver: function DirectoryLinksProvider_addObserver() {
    for (let pref in this._observedPrefs) {
      let prefName = this._observedPrefs[pref];
      Services.prefs.addObserver(prefName, this, false);
    }
  },

  _removePrefsObserver: function DirectoryLinksProvider_removeObserver() {
    for (let pref in this._observedPrefs) {
      let prefName = this._observedPrefs[pref];
      Services.prefs.removeObserver(prefName, this);
    }
  },

  _cacheRelatedLinks: function(link) {
    for (let relatedSite of link.related) {
      let relatedMap = this._relatedLinks.get(relatedSite) || new Map();
      relatedMap.set(link.url, link);
      this._relatedLinks.set(relatedSite, relatedMap);
    }
  },

  _fetchAndCacheLinks: function DirectoryLinksProvider_fetchAndCacheLinks(uri) {
    // Replace with the same display locale used for selecting links data
    uri = uri.replace("%LOCALE%", this.locale);

    let deferred = Promise.defer();
    let xmlHttp = new XMLHttpRequest();

    let self = this;
    xmlHttp.onload = function(aResponse) {
      let json = this.responseText;
      if (this.status && this.status != 200) {
        json = "{}";
      }
      OS.File.writeAtomic(self._directoryFilePath, json, {tmpPath: self._directoryFilePath + ".tmp"})
        .then(() => {
          deferred.resolve();
        },
        () => {
          deferred.reject("Error writing uri data in profD.");
        });
    };

    xmlHttp.onerror = function(e) {
      deferred.reject("Fetching " + uri + " results in error code: " + e.target.status);
    };

    try {
      xmlHttp.open("GET", uri);
      // Override the type so XHR doesn't complain about not well-formed XML
      xmlHttp.overrideMimeType(DIRECTORY_LINKS_TYPE);
      // Set the appropriate request type for servers that require correct types
      xmlHttp.setRequestHeader("Content-Type", DIRECTORY_LINKS_TYPE);
      xmlHttp.send();
    } catch (e) {
      deferred.reject("Error fetching " + uri);
      Cu.reportError(e);
    }
    return deferred.promise;
  },

  /**
   * Downloads directory links if needed
   * @return promise resolved immediately if no download needed, or upon completion
   */
  _fetchAndCacheLinksIfNecessary: function DirectoryLinksProvider_fetchAndCacheLinksIfNecessary(forceDownload=false) {
    if (this._downloadDeferred) {
      // fetching links already - just return the promise
      return this._downloadDeferred.promise;
    }

    if (forceDownload || this._needsDownload) {
      this._downloadDeferred = Promise.defer();
      this._fetchAndCacheLinks(this._linksURL).then(() => {
        // the new file was successfully downloaded and cached, so update a timestamp
        this._lastDownloadMS = Date.now();
        this._downloadDeferred.resolve();
        this._downloadDeferred = null;
        this._callObservers("onManyLinksChanged")
      },
      error => {
        this._downloadDeferred.resolve();
        this._downloadDeferred = null;
        this._callObservers("onDownloadFail");
      });
      return this._downloadDeferred.promise;
    }

    // download is not needed
    return Promise.resolve();
  },

  /**
   * @return true if download is needed, false otherwise
   */
  get _needsDownload () {
    // fail if last download occured less then 24 hours ago
    if ((Date.now() - this._lastDownloadMS) > this._downloadIntervalMS) {
      return true;
    }
    return false;
  },

  /**
   * Reads directory links file and parses its content
   * @return a promise resolved to valid list of links or [] if read or parse fails
   */
  _readDirectoryLinksFile: function DirectoryLinksProvider_readDirectoryLinksFile() {
    return OS.File.read(this._directoryFilePath).then(binaryData => {
      let output;
      try {
        let locale = this.locale;
        let json = gTextDecoder.decode(binaryData);
        let list = JSON.parse(json);
        output = list[locale];
      }
      catch (e) {
        Cu.reportError(e);
      }
      return output || [];
    },
    error => {
      Cu.reportError(error);
      return [];
    });
  },

  /**
   * Report some action on a newtab page (view, click)
   * @param sites Array of sites shown on newtab page
   * @param action String of the behavior to report
   * @param triggeringSiteIndex optional Int index of the site triggering action
   * @return download promise
   */
  reportSitesAction: function DirectoryLinksProvider_reportSitesAction(sites, action, triggeringSiteIndex) {
    let newtabEnhanced = false;
    let pingEndPoint = "";
    try {
      newtabEnhanced = Services.prefs.getBoolPref(PREF_NEWTAB_ENHANCED);
      pingEndPoint = Services.prefs.getCharPref(PREF_DIRECTORY_PING);
    }
    catch (ex) {}

    // Only send pings when enhancing tiles with an endpoint and valid action
    let invalidAction = PING_ACTIONS.indexOf(action) == -1;
    if (!newtabEnhanced || pingEndPoint == "" || invalidAction) {
      return Promise.resolve();
    }

    let actionIndex;
    let data = {
      locale: this.locale,
      tiles: sites.reduce((tiles, site, pos) => {
        // Only add data for non-empty tiles
        if (site) {
          // Remember which tiles data triggered the action
          let {link} = site;
          let tilesIndex = tiles.length;
          if (triggeringSiteIndex == pos) {
            actionIndex = tilesIndex;
          }

          // Make the payload in a way so keys can be excluded when stringified
          let id = link.directoryId;
          tiles.push({
            id: id || site.enhancedId,
            pin: site.isPinned() ? 1 : undefined,
            pos: pos != tilesIndex ? pos : undefined,
            score: Math.round(link.frecency / PING_SCORE_DIVISOR) || undefined,
            url: site.enhancedId && "",
          });
        }
        return tiles;
      }, []),
    };

    // Provide a direct index to the tile triggering the action
    if (actionIndex !== undefined) {
      data[action] = actionIndex;
    }

    // Package the data to be sent with the ping
    let ping = new XMLHttpRequest();
    ping.open("POST", pingEndPoint + (action == "view" ? "view" : "click"));
    ping.send(JSON.stringify(data));

    // Use this as an opportunity to potentially fetch new links
    return this._fetchAndCacheLinksIfNecessary();
  },

  /**
   * Get the enhanced link object for a link (whether history or directory)
   */
  getEnhancedLink: function DirectoryLinksProvider_getEnhancedLink(link) {
    // Use the provided link if it's already enhanced
    return link.enhancedImageURI && link ||
           this._enhancedLinks.get(NewTabUtils.extractSite(link.url));
  },

  /**
   * Check if a url's scheme is in a Set of allowed schemes
   */
  isURLAllowed: function DirectoryLinksProvider_isURLAllowed(url, allowed) {
    // Assume no url is an allowed url
    if (!url) {
      return true;
    }

    let scheme = "";
    try {
      // A malformed url will not be allowed
      scheme = Services.io.newURI(url, null, null).scheme;
    }
    catch(ex) {}
    return allowed.has(scheme);
  },

  /**
   * Gets the current set of directory links.
   * @param aCallback The function that the array of links is passed to.
   */
  getLinks: function DirectoryLinksProvider_getLinks(aCallback) {
    this._readDirectoryLinksFile().then(rawLinks => {
      // Reset the cache of related tiles and enhanced images for this new set of links
      this._enhancedLinks.clear();
      this._relatedLinks.clear();

      let links = [];
      rawLinks.filter(link => {
        // Make sure the link url is allowed and images too if they exist
        return this.isURLAllowed(link.url, ALLOWED_LINK_SCHEMES) &&
               this.isURLAllowed(link.imageURI, ALLOWED_IMAGE_SCHEMES) &&
               this.isURLAllowed(link.enhancedImageURI, ALLOWED_IMAGE_SCHEMES);
      }).forEach((link, position) => {
        // Stash the enhanced image for the site
        if (link.enhancedImageURI) {
          this._enhancedLinks.set(NewTabUtils.extractSite(link.url), link);
        }
        link.lastVisitDate = rawLinks.length - position;

        // We cache related tiles here but do not push any of them in the links list yet.
        // The decision for which related tile to include will be made separately.
        if ("related" == link.type) {
          this._cacheRelatedLinks(link);
          return;
        }
        link.frecency = DIRECTORY_FRECENCY;
        links.push(link);
      });
      return links;
    }).catch(ex => {
      Cu.reportError(ex);
      return [];
    }).then(links => {
      aCallback(links);
      this._populatePlacesLinks();
    });
  },

  init: function DirectoryLinksProvider_init() {
    this._setDefaultEnhanced();
    this._addPrefsObserver();
    // setup directory file path and last download timestamp
    this._directoryFilePath = OS.Path.join(OS.Constants.Path.localProfileDir, DIRECTORY_LINKS_FILE);
    this._lastDownloadMS = 0;

    NewTabUtils.placesProvider.addObserver(this);

    return Task.spawn(function() {
      // get the last modified time of the links file if it exists
      let doesFileExists = yield OS.File.exists(this._directoryFilePath);
      if (doesFileExists) {
        let fileInfo = yield OS.File.stat(this._directoryFilePath);
        this._lastDownloadMS = Date.parse(fileInfo.lastModificationDate);
      }
      // fetch directory on startup without force
      yield this._fetchAndCacheLinksIfNecessary();
    }.bind(this));
  },

  _handleManyLinksChanged: function() {
    this._topSitesWithRelatedLinks.clear();
    this._relatedLinks.forEach((relatedLinks, site) => {
      if (NewTabUtils.isTopPlacesSite(site)) {
        this._topSitesWithRelatedLinks.add(site);
      }
    });
    this._updateRelatedTile();
  },

  /**
   * Updates _topSitesWithRelatedLinks based on the link that was changed.
   *
   * @return true if _topSitesWithRelatedLinks was modified, false otherwise.
   */
  _handleLinkChanged: function(aLink) {
    let changedLinkSite = NewTabUtils.extractSite(aLink.url);
    let linkStored = this._topSitesWithRelatedLinks.has(changedLinkSite);

    if (!NewTabUtils.isTopPlacesSite(changedLinkSite) && linkStored) {
      this._topSitesWithRelatedLinks.delete(changedLinkSite);
      return true;
    }

    if (this._relatedLinks.has(changedLinkSite) &&
        NewTabUtils.isTopPlacesSite(changedLinkSite) && !linkStored) {
      this._topSitesWithRelatedLinks.add(changedLinkSite);
      return true;
    }
    return false;
  },

  _populatePlacesLinks: function () {
    NewTabUtils.links.populateProviderCache(NewTabUtils.placesProvider, () => {
      this._handleManyLinksChanged();
    });
  },

  onLinkChanged: function (aProvider, aLink) {
    // Make sure NewTabUtils.links handles the notification first.
    setTimeout(() => {
      if (this._handleLinkChanged(aLink)) {
        this._updateRelatedTile();
      }
    }, 0);
  },

  onManyLinksChanged: function () {
    // Make sure NewTabUtils.links handles the notification first.
    setTimeout(() => {
      this._handleManyLinksChanged();
    }, 0);
  },

  /**
   * Chooses and returns a related tile based on a user's top sites
   * that we have an available related tile for.
   *
   * @return the chosen related tile, or undefined if there isn't one
   */
  _updateRelatedTile: function() {
    let sortedLinks = NewTabUtils.getProviderLinks(this);

    if (!sortedLinks) {
      // If NewTabUtils.links.resetCache() is called before getting here,
      // sortedLinks may be undefined.
      return;
    }

    // Delete the current related tile, if one exists.
    let initialLength = sortedLinks.length;
    this.maxNumLinks = initialLength;
    if (initialLength) {
      let mostFrecentLink = sortedLinks[0];
      if ("related" == mostFrecentLink.type) {
        this._callObservers("onLinkChanged", {
          url: mostFrecentLink.url,
          frecency: 0,
          lastVisitDate: mostFrecentLink.lastVisitDate,
          type: "related",
        }, 0, true);
      }
    }

    if (this._topSitesWithRelatedLinks.size == 0) {
      // There are no potential related links we can show.
      return;
    }

    // Create a flat list of all possible links we can show as related.
    // Note that many top sites may map to the same related links, but we only
    // want to count each related link once (based on url), thus possibleLinks is a map
    // from url to relatedLink. Thus, each link has an equal chance of being chosen at
    // random from flattenedLinks if it appears only once.
    let possibleLinks = new Map();
    let targetedSites = new Map();
    this._topSitesWithRelatedLinks.forEach(topSiteWithRelatedLink => {
      let relatedLinksMap = this._relatedLinks.get(topSiteWithRelatedLink);
      relatedLinksMap.forEach((relatedLink, url) => {
        possibleLinks.set(url, relatedLink);

        // Keep a map of URL to targeted sites. We later use this to show the user
        // what site they visited to trigger this suggestion.
        if (!targetedSites.get(url)) {
          targetedSites.set(url, []);
        }
        targetedSites.get(url).push(topSiteWithRelatedLink);
      })
    });
    let flattenedLinks = [...possibleLinks.values()];

    // Choose our related link at random
    let relatedIndex = Math.floor(Math.random() * flattenedLinks.length);
    let chosenRelatedLink = flattenedLinks[relatedIndex];

    // Show the new directory tile.
    this._callObservers("onLinkChanged", {
      url: chosenRelatedLink.url,
      title: chosenRelatedLink.title,
      frecency: RELATED_FRECENCY,
      lastVisitDate: chosenRelatedLink.lastVisitDate,
      type: "related",

      // Choose the first site a user has visited as the target. In the future,
      // this should be the site with the highest frecency. However, we currently
      // store frecency by URL not by site.
      targetedSite: targetedSites.get(chosenRelatedLink.url).length ?
        targetedSites.get(chosenRelatedLink.url)[0] : null
    });
    return chosenRelatedLink;
   },

  /**
   * Return the object to its pre-init state
   */
  reset: function DirectoryLinksProvider_reset() {
    delete this.__linksURL;
    this._removePrefsObserver();
    this._removeObservers();
  },

  addObserver: function DirectoryLinksProvider_addObserver(aObserver) {
    this._observers.add(aObserver);
  },

  removeObserver: function DirectoryLinksProvider_removeObserver(aObserver) {
    this._observers.delete(aObserver);
  },

  _callObservers: function DirectoryLinksProvider__callObservers(aMethodName, aArg) {
    for (let obs of this._observers) {
      if (typeof(obs[aMethodName]) == "function") {
        try {
          obs[aMethodName](this, aArg);
        } catch (err) {
          Cu.reportError(err);
        }
      }
    }
  },

  _removeObservers: function() {
    this._observers.clear();
  }
};
