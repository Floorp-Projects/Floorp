/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["DirectoryLinksProvider"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const ParserUtils =  Cc["@mozilla.org/parserutils;1"].getService(Ci.nsIParserUtils);

Cu.importGlobalProperties(["XMLHttpRequest"]);

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
XPCOMUtils.defineLazyModuleGetter(this, "UpdateUtils",
  "resource://gre/modules/UpdateUtils.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "eTLD",
  "@mozilla.org/network/effective-tld-service;1",
  "nsIEffectiveTLDService");
XPCOMUtils.defineLazyGetter(this, "gTextDecoder", () => {
  return new TextDecoder();
});
XPCOMUtils.defineLazyGetter(this, "gCryptoHash", function () {
  return Cc["@mozilla.org/security/hash;1"].createInstance(Ci.nsICryptoHash);
});
XPCOMUtils.defineLazyGetter(this, "gUnicodeConverter", function () {
  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = 'utf8';
  return converter;
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

// Only allow urls to Mozilla's CDN or empty (for data URIs)
const ALLOWED_URL_BASE = new Set(["mozilla.net", ""]);

// The frecency of a directory link
const DIRECTORY_FRECENCY = 1000;

// The frecency of a suggested link
const SUGGESTED_FRECENCY = Infinity;

// The filename where frequency cap data stored locally
const FREQUENCY_CAP_FILE = "frequencyCap.json";

// Default settings for daily and total frequency caps
const DEFAULT_DAILY_FREQUENCY_CAP = 3;
const DEFAULT_TOTAL_FREQUENCY_CAP = 10;

// Default timeDelta to prune unused frequency cap objects
// currently set to 10 days in milliseconds
const DEFAULT_PRUNE_TIME_DELTA = 10*24*60*60*1000;

// The min number of visible (not blocked) history tiles to have before showing suggested tiles
const MIN_VISIBLE_HISTORY_TILES = 8;

// The max number of visible (not blocked) history tiles to test for inadjacency
const MAX_VISIBLE_HISTORY_TILES = 15;

// Allowed ping actions remotely stored as columns: case-insensitive [a-z0-9_]
const PING_ACTIONS = ["block", "click", "pin", "sponsored", "sponsored_link", "unpin", "view"];

// Location of inadjacent sites json
const INADJACENCY_SOURCE = "chrome://browser/content/newtab/newTab.inadjacent.json";

// Fake URL to keep track of last block of a suggested tile in the frequency cap object
const FAKE_SUGGESTED_BLOCK_URL = "ignore://suggested_block";

// Time before suggested tile is allowed to play again after block - default to 1 day
const AFTER_SUGGESTED_BLOCK_DECAY_TIME = 24*60*60*1000;

/**
 * Singleton that serves as the provider of directory links.
 * Directory links are a hard-coded set of links shown if a user's link
 * inventory is empty.
 */
var DirectoryLinksProvider = {

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
   * A mapping from site to a list of suggested link objects
   */
  _suggestedLinks: new Map(),

  /**
   * Frequency Cap object - maintains daily and total tile counts, and frequency cap settings
   */
  _frequencyCaps: {},

  /**
   * A set of top sites that we can provide suggested links for
   */
  _topSitesWithSuggestedLinks: new Set(),

  /**
   * lookup Set of inadjacent domains
   */
  _inadjacentSites: new Set(),

  /**
   * This flag is set if there is a suggested tile configured to avoid
   * inadjacent sites in new tab
   */
  _avoidInadjacentSites: false,

  /**
   * This flag is set if _avoidInadjacentSites is true and there is
   * an inadjacent site in the new tab
   */
  _newTabHasInadjacentSite: false,

  get _observedPrefs() {
    return Object.freeze({
      enhanced: PREF_NEWTAB_ENHANCED,
      linksURL: PREF_DIRECTORY_SOURCE,
      matchOSLocale: PREF_MATCH_OS_LOCALE,
      prefSelectedLocale: PREF_SELECTED_LOCALE,
    });
  },

  get _linksURL() {
    if (!this.__linksURL) {
      try {
        this.__linksURL = Services.prefs.getCharPref(this._observedPrefs["linksURL"]);
        this.__linksURLModified = Services.prefs.prefHasUserValue(this._observedPrefs["linksURL"]);
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
      let enhanced = Services.prefs.getBoolPref(PREF_NEWTAB_ENHANCED);
      try {
        // Default to not enhanced if DNT is set to tell websites to not track
        if (Services.prefs.getBoolPref("privacy.donottrackheader.enabled")) {
          enhanced = false;
        }
      }
      catch (ex) {}
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

  _cacheSuggestedLinks: function(link) {
    // Don't cache links that don't have the expected 'frecent_sites'
    if (!link.frecent_sites) {
      return;
    }

    for (let suggestedSite of link.frecent_sites) {
      let suggestedMap = this._suggestedLinks.get(suggestedSite) || new Map();
      suggestedMap.set(link.url, link);
      this._setupStartEndTime(link);
      this._suggestedLinks.set(suggestedSite, suggestedMap);
    }
  },

  _fetchAndCacheLinks: function DirectoryLinksProvider_fetchAndCacheLinks(uri) {
    // Replace with the same display locale used for selecting links data
    uri = uri.replace("%LOCALE%", this.locale);
    uri = uri.replace("%CHANNEL%", UpdateUtils.UpdateChannel);

    return this._downloadJsonData(uri).then(json => {
      return OS.File.writeAtomic(this._directoryFilePath, json, {tmpPath: this._directoryFilePath + ".tmp"});
    });
  },

  /**
   * Downloads a links with json content
   * @param download uri
   * @return promise resolved to json string, "{}" returned if status != 200
   */
  _downloadJsonData: function DirectoryLinksProvider__downloadJsonData(uri) {
    let deferred = Promise.defer();
    let xmlHttp = this._newXHR();

    xmlHttp.onload = function(aResponse) {
      let json = this.responseText;
      if (this.status && this.status != 200) {
        json = "{}";
      }
      deferred.resolve(json);
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
   * Create a new XMLHttpRequest that is anonymous, i.e., doesn't send cookies
   */
  _newXHR() {
    return new XMLHttpRequest({mozAnon: true});
  },

  /**
   * Reads directory links file and parses its content
   * @return a promise resolved to an object with keys 'directory' and 'suggested',
   *         each containing a valid list of links,
   *         or {'directory': [], 'suggested': []} if read or parse fails.
   */
  _readDirectoryLinksFile: function DirectoryLinksProvider_readDirectoryLinksFile() {
    let emptyOutput = {directory: [], suggested: [], enhanced: []};
    return OS.File.read(this._directoryFilePath).then(binaryData => {
      let output;
      try {
        let json = gTextDecoder.decode(binaryData);
        let linksObj = JSON.parse(json);
        output = {directory: linksObj.directory || [],
                  suggested: linksObj.suggested || [],
                  enhanced:  linksObj.enhanced  || []};
      }
      catch (e) {
        Cu.reportError(e);
      }
      return output || emptyOutput;
    },
    error => {
      Cu.reportError(error);
      return emptyOutput;
    });
  },

  /**
   * Translates link.time_limits to UTC miliseconds and sets
   * link.startTime and link.endTime properties in link object
   */
  _setupStartEndTime: function DirectoryLinksProvider_setupStartEndTime(link) {
    // set start/end limits. Use ISO_8601 format: '2014-01-10T20:20:20.600Z'
    // (details here http://en.wikipedia.org/wiki/ISO_8601)
    // Note that if timezone is missing, FX will interpret as local time
    // meaning that the server can sepecify any time, but if the capmaign
    // needs to start at same time across multiple timezones, the server
    // omits timezone indicator
    if (!link.time_limits) {
      return;
    }

    let parsedTime;
    if (link.time_limits.start) {
      parsedTime = Date.parse(link.time_limits.start);
      if (parsedTime && !isNaN(parsedTime)) {
        link.startTime = parsedTime;
      }
    }
    if (link.time_limits.end) {
      parsedTime = Date.parse(link.time_limits.end);
      if (parsedTime && !isNaN(parsedTime)) {
        link.endTime = parsedTime;
      }
    }
  },

  /*
   * Handles campaign timeout
   */
  _onCampaignTimeout: function DirectoryLinksProvider_onCampaignTimeout() {
    // _campaignTimeoutID is invalid here, so just set it to null
    this._campaignTimeoutID = null;
    this._updateSuggestedTile();
  },

  /*
   * Clears capmpaign timeout
   */
  _clearCampaignTimeout: function DirectoryLinksProvider_clearCampaignTimeout() {
    if (this._campaignTimeoutID) {
      clearTimeout(this._campaignTimeoutID);
      this._campaignTimeoutID = null;
    }
  },

  /**
   * Setup capmpaign timeout to recompute suggested tiles upon
   * reaching soonest start or end time for the campaign
   * @param timeout in milliseconds
   */
  _setupCampaignTimeCheck: function DirectoryLinksProvider_setupCampaignTimeCheck(timeout) {
    // sanity check
    if (!timeout || timeout <= 0) {
      return;
    }
    this._clearCampaignTimeout();
    // setup next timeout
    this._campaignTimeoutID = setTimeout(this._onCampaignTimeout.bind(this), timeout);
  },

  /**
   * Test link for campaign time limits: checks if link falls within start/end time
   * and returns an object containing a use flag and the timeoutDate milliseconds
   * when the link has to be re-checked for campaign start-ready or end-reach
   * @param link
   * @return object {use: true or false, timeoutDate: milliseconds or null}
   */
  _testLinkForCampaignTimeLimits: function DirectoryLinksProvider_testLinkForCampaignTimeLimits(link) {
    let currentTime = Date.now();
    // test for start time first
    if (link.startTime && link.startTime > currentTime) {
      // not yet ready for start
      return {use: false, timeoutDate: link.startTime};
    }
    // otherwise check for end time
    if (link.endTime) {
      // passed end time
      if (link.endTime <= currentTime) {
        return {use: false};
      }
      // otherwise link is still ok, but we need to set timeoutDate
      return {use: true, timeoutDate: link.endTime};
    }
    // if we are here, the link is ok and no timeoutDate needed
    return {use: true};
  },

  /**
   * Handles block on suggested tile: updates fake block url with current timestamp
   */
  handleSuggestedTileBlock: function DirectoryLinksProvider_handleSuggestedTileBlock() {
    this._updateFrequencyCapSettings({url: FAKE_SUGGESTED_BLOCK_URL});
    this._writeFrequencyCapFile();
    this._updateSuggestedTile();
  },

  /**
   * Checks if suggested tile is being blocked for the rest of "decay time"
   * @return True if blocked, false otherwise
   */
  _isSuggestedTileBlocked: function DirectoryLinksProvider__isSuggestedTileBlocked() {
    let capObject = this._frequencyCaps[FAKE_SUGGESTED_BLOCK_URL];
    if (!capObject || !capObject.lastUpdated) {
      // user never blocked suggested tile or lastUpdated is missing
      return false;
    }
    // otherwise, make sure that enough time passed after suggested tile was blocked
    return (capObject.lastUpdated + AFTER_SUGGESTED_BLOCK_DECAY_TIME) > Date.now();
  },

  /**
   * Report some action on a newtab page (view, click)
   * @param sites Array of sites shown on newtab page
   * @param action String of the behavior to report
   * @param triggeringSiteIndex optional Int index of the site triggering action
   * @return download promise
   */
  reportSitesAction: function DirectoryLinksProvider_reportSitesAction(sites, action, triggeringSiteIndex) {
    let pastImpressions;
    // Check if the suggested tile was shown
    if (action == "view") {
      sites.slice(0, triggeringSiteIndex + 1).filter(s => s).forEach(site => {
        let {targetedSite, url} = site.link;
        if (targetedSite) {
          this._addFrequencyCapView(url);
        }
      });
    }
    // any click action on a suggested tile should stop that tile suggestion
    // click/block - user either removed a tile or went to a landing page
    // pin - tile turned into history tile, should no longer be suggested
    // unpin - the tile was pinned before, should not matter
    else {
      // suggested tile has targetedSite, or frecent_sites if it was pinned
      let {frecent_sites, targetedSite, url} = sites[triggeringSiteIndex].link;
      if (frecent_sites || targetedSite) {
        // skip past_impressions for "unpin" to avoid chance of tracking
        if (this._frequencyCaps[url] && action != "unpin") {
          pastImpressions = {
            total: this._frequencyCaps[url].totalViews,
            daily: this._frequencyCaps[url].dailyViews
          };
        }
        this._setFrequencyCapClick(url);
      }
    }

    let newtabEnhanced = false;
    let pingEndPoint = "";
    try {
      newtabEnhanced = Services.prefs.getBoolPref(PREF_NEWTAB_ENHANCED);
      pingEndPoint = Services.prefs.getCharPref(PREF_DIRECTORY_PING);
    }
    catch (ex) {}

    // Bug 1240245 - We no longer send pings, but frequency capping and fetching
    // tests depend on the following actions, so references to PING remain.
    let invalidAction = PING_ACTIONS.indexOf(action) == -1;
    if (!newtabEnhanced || pingEndPoint == "" || invalidAction) {
      return Promise.resolve();
    }

    return Task.spawn(function* () {
      // since we updated views/clicks we need write _frequencyCaps to disk
      yield this._writeFrequencyCapFile();
      // Use this as an opportunity to potentially fetch new links
      yield this._fetchAndCacheLinksIfNecessary();
    }.bind(this));
  },

  /**
   * Get the enhanced link object for a link (whether history or directory)
   */
  getEnhancedLink: function DirectoryLinksProvider_getEnhancedLink(link) {
    // Use the provided link if it's already enhanced
    return link.enhancedImageURI && link ? link :
           this._enhancedLinks.get(NewTabUtils.extractSite(link.url));
  },

  /**
   * Check if a url's scheme is in a Set of allowed schemes and if the base
   * domain is allowed.
   * @param url to check
   * @param allowed Set of allowed schemes
   * @param checkBase boolean to check the base domain
   */
  isURLAllowed(url, allowed, checkBase) {
    // Assume no url is an allowed url
    if (!url) {
      return true;
    }

    let scheme = "", base = "";
    try {
      // A malformed url will not be allowed
      let uri = Services.io.newURI(url, null, null);
      scheme = uri.scheme;

      // URIs without base domains will be allowed
      base = Services.eTLD.getBaseDomain(uri);
    }
    catch (ex) {}
    // Require a scheme match and the base only if desired
    return allowed.has(scheme) && (!checkBase || ALLOWED_URL_BASE.has(base));
  },

  _escapeChars(text) {
    let charMap = {
      '&': '&amp;',
      '<': '&lt;',
      '>': '&gt;',
      '"': '&quot;',
      "'": '&#039;'
    };

    return text.replace(/[&<>"']/g, (character) => charMap[character]);
  },

  /**
   * Gets the current set of directory links.
   * @param aCallback The function that the array of links is passed to.
   */
  getLinks: function DirectoryLinksProvider_getLinks(aCallback) {
    this._readDirectoryLinksFile().then(rawLinks => {
      // Reset the cache of suggested tiles and enhanced images for this new set of links
      this._enhancedLinks.clear();
      this._suggestedLinks.clear();
      this._clearCampaignTimeout();
      this._avoidInadjacentSites = false;

      // Only check base domain for images when using the default pref
      let checkBase = !this.__linksURLModified;
      let validityFilter = function(link) {
        // Make sure the link url is allowed and images too if they exist
        return this.isURLAllowed(link.url, ALLOWED_LINK_SCHEMES, false) &&
               (!link.imageURI ||
                this.isURLAllowed(link.imageURI, ALLOWED_IMAGE_SCHEMES, checkBase)) &&
               (!link.enhancedImageURI ||
                this.isURLAllowed(link.enhancedImageURI, ALLOWED_IMAGE_SCHEMES, checkBase));
      }.bind(this);

      rawLinks.suggested.filter(validityFilter).forEach((link, position) => {
        // Suggested sites must have an adgroup name.
        if (!link.adgroup_name) {
          return;
        }

        let sanitizeFlags = ParserUtils.SanitizerCidEmbedsOnly |
          ParserUtils.SanitizerDropForms |
          ParserUtils.SanitizerDropNonCSSPresentation;

        link.explanation = this._escapeChars(link.explanation ? ParserUtils.convertToPlainText(link.explanation, sanitizeFlags, 0) : "");
        link.targetedName = this._escapeChars(ParserUtils.convertToPlainText(link.adgroup_name, sanitizeFlags, 0));
        link.lastVisitDate = rawLinks.suggested.length - position;
        // check if link wants to avoid inadjacent sites
        if (link.check_inadjacency) {
          this._avoidInadjacentSites = true;
        }

        // We cache suggested tiles here but do not push any of them in the links list yet.
        // The decision for which suggested tile to include will be made separately.
        this._cacheSuggestedLinks(link);
        this._updateFrequencyCapSettings(link);
      });

      rawLinks.enhanced.filter(validityFilter).forEach((link, position) => {
        link.lastVisitDate = rawLinks.enhanced.length - position;

        // Stash the enhanced image for the site
        if (link.enhancedImageURI) {
          this._enhancedLinks.set(NewTabUtils.extractSite(link.url), link);
        }
      });

      let links = rawLinks.directory.filter(validityFilter).map((link, position) => {
        link.lastVisitDate = rawLinks.directory.length - position;
        link.frecency = DIRECTORY_FRECENCY;
        return link;
      });

      // Allow for one link suggestion on top of the default directory links
      this.maxNumLinks = links.length + 1;

      // prune frequency caps of outdated urls
      this._pruneFrequencyCapUrls();
      // write frequency caps object to disk asynchronously
      this._writeFrequencyCapFile();

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

    // setup frequency cap file path
    this._frequencyCapFilePath = OS.Path.join(OS.Constants.Path.localProfileDir, FREQUENCY_CAP_FILE);
    // setup inadjacent sites URL
    this._inadjacentSitesUrl = INADJACENCY_SOURCE;

    NewTabUtils.placesProvider.addObserver(this);
    NewTabUtils.links.addObserver(this);

    return Task.spawn(function*() {
      // get the last modified time of the links file if it exists
      let doesFileExists = yield OS.File.exists(this._directoryFilePath);
      if (doesFileExists) {
        let fileInfo = yield OS.File.stat(this._directoryFilePath);
        this._lastDownloadMS = Date.parse(fileInfo.lastModificationDate);
      }
      // read frequency cap file
      yield this._readFrequencyCapFile();
      // fetch directory on startup without force
      yield this._fetchAndCacheLinksIfNecessary();
      // fecth inadjacent sites on startup
      yield this._loadInadjacentSites();
    }.bind(this));
  },

  _handleManyLinksChanged: function() {
    this._topSitesWithSuggestedLinks.clear();
    this._suggestedLinks.forEach((suggestedLinks, site) => {
      if (NewTabUtils.isTopPlacesSite(site)) {
        this._topSitesWithSuggestedLinks.add(site);
      }
    });
    this._updateSuggestedTile();
  },

  /**
   * Updates _topSitesWithSuggestedLinks based on the link that was changed.
   *
   * @return true if _topSitesWithSuggestedLinks was modified, false otherwise.
   */
  _handleLinkChanged: function(aLink) {
    let changedLinkSite = NewTabUtils.extractSite(aLink.url);
    let linkStored = this._topSitesWithSuggestedLinks.has(changedLinkSite);

    if (!NewTabUtils.isTopPlacesSite(changedLinkSite) && linkStored) {
      this._topSitesWithSuggestedLinks.delete(changedLinkSite);
      return true;
    }

    if (this._suggestedLinks.has(changedLinkSite) &&
        NewTabUtils.isTopPlacesSite(changedLinkSite) && !linkStored) {
      this._topSitesWithSuggestedLinks.add(changedLinkSite);
      return true;
    }

    // always run _updateSuggestedTile if aLink is inadjacent
    // and there are tiles configured to avoid it
    if (this._avoidInadjacentSites && this._isInadjacentLink(aLink)) {
      return true;
    }

    return false;
  },

  _populatePlacesLinks: function () {
    NewTabUtils.links.populateProviderCache(NewTabUtils.placesProvider, () => {
      this._handleManyLinksChanged();
    });
  },

  onDeleteURI: function(aProvider, aLink) {
    let {url} = aLink;
    // remove clicked flag for that url and
    // call observer upon disk write completion
    this._removeTileClick(url).then(() => {
      this._callObservers("onDeleteURI", url);
    });
  },

  onClearHistory: function() {
    // remove all clicked flags and call observers upon file write
    this._removeAllTileClicks().then(() => {
      this._callObservers("onClearHistory");
    });
  },

  onLinkChanged: function (aProvider, aLink) {
    // Make sure NewTabUtils.links handles the notification first.
    setTimeout(() => {
      if (this._handleLinkChanged(aLink) || this._shouldUpdateSuggestedTile()) {
        this._updateSuggestedTile();
      }
    }, 0);
  },

  onManyLinksChanged: function () {
    // Make sure NewTabUtils.links handles the notification first.
    setTimeout(() => {
      this._handleManyLinksChanged();
    }, 0);
  },

  _getCurrentTopSiteCount: function() {
    let visibleTopSiteCount = 0;
    let newTabLinks = NewTabUtils.links.getLinks();
    for (let link of newTabLinks.slice(0, MIN_VISIBLE_HISTORY_TILES)) {
      // compute visibleTopSiteCount for suggested tiles
      if (link && (link.type == "history" || link.type == "enhanced")) {
        visibleTopSiteCount++;
      }
    }
    // since newTabLinks are available, set _newTabHasInadjacentSite here
    // note that _shouldUpdateSuggestedTile is called by _updateSuggestedTile
    this._newTabHasInadjacentSite = this._avoidInadjacentSites && this._checkForInadjacentSites(newTabLinks);

    return visibleTopSiteCount;
  },

  _shouldUpdateSuggestedTile: function() {
    let sortedLinks = NewTabUtils.getProviderLinks(this);

    let mostFrecentLink = {};
    if (sortedLinks && sortedLinks.length) {
      mostFrecentLink = sortedLinks[0]
    }

    let currTopSiteCount = this._getCurrentTopSiteCount();
    if ((!mostFrecentLink.targetedSite && currTopSiteCount >= MIN_VISIBLE_HISTORY_TILES) ||
        (mostFrecentLink.targetedSite && currTopSiteCount < MIN_VISIBLE_HISTORY_TILES)) {
      // If mostFrecentLink has a targetedSite then mostFrecentLink is a suggested link.
      // If we have enough history links (8+) to show a suggested tile and we are not
      // already showing one, then we should update (to *attempt* to add a suggested tile).
      // OR if we don't have enough history to show a suggested tile (<8) and we are
      // currently showing one, we should update (to remove it).
      return true;
    }

    return false;
  },

  /**
   * Chooses and returns a suggested tile based on a user's top sites
   * that we have an available suggested tile for.
   *
   * @return the chosen suggested tile, or undefined if there isn't one
   */
  _updateSuggestedTile: function() {
    let sortedLinks = NewTabUtils.getProviderLinks(this);

    if (!sortedLinks) {
      // If NewTabUtils.links.resetCache() is called before getting here,
      // sortedLinks may be undefined.
      return undefined;
    }

    // Delete the current suggested tile, if one exists.
    let initialLength = sortedLinks.length;
    if (initialLength) {
      let mostFrecentLink = sortedLinks[0];
      if (mostFrecentLink.targetedSite) {
        this._callObservers("onLinkChanged", {
          url: mostFrecentLink.url,
          frecency: SUGGESTED_FRECENCY,
          lastVisitDate: mostFrecentLink.lastVisitDate,
          type: mostFrecentLink.type,
        }, 0, true);
      }
    }

    if (this._topSitesWithSuggestedLinks.size == 0 ||
        !this._shouldUpdateSuggestedTile() ||
        this._isSuggestedTileBlocked()) {
      // There are no potential suggested links we can show or not
      // enough history for a suggested tile, or suggested tile was
      // recently blocked and wait time interval has not decayed yet
      return undefined;
    }

    // Create a flat list of all possible links we can show as suggested.
    // Note that many top sites may map to the same suggested links, but we only
    // want to count each suggested link once (based on url), thus possibleLinks is a map
    // from url to suggestedLink. Thus, each link has an equal chance of being chosen at
    // random from flattenedLinks if it appears only once.
    let nextTimeout;
    let possibleLinks = new Map();
    let targetedSites = new Map();
    this._topSitesWithSuggestedLinks.forEach(topSiteWithSuggestedLink => {
      let suggestedLinksMap = this._suggestedLinks.get(topSiteWithSuggestedLink);
      suggestedLinksMap.forEach((suggestedLink, url) => {
        // Skip this link if we've shown it too many times already
        if (!this._testFrequencyCapLimits(url)) {
          return;
        }

        // as we iterate suggestedLinks, check for campaign start/end
        // time limits, and set nextTimeout to the closest timestamp
        let {use, timeoutDate} = this._testLinkForCampaignTimeLimits(suggestedLink);
        // update nextTimeout is necessary
        if (timeoutDate && (!nextTimeout || nextTimeout > timeoutDate)) {
          nextTimeout = timeoutDate;
        }
        // Skip link if it falls outside campaign time limits
        if (!use) {
          return;
        }

        // Skip link if it avoids inadjacent sites and newtab has one
        if (suggestedLink.check_inadjacency && this._newTabHasInadjacentSite) {
          return;
        }

        possibleLinks.set(url, suggestedLink);

        // Keep a map of URL to targeted sites. We later use this to show the user
        // what site they visited to trigger this suggestion.
        if (!targetedSites.get(url)) {
          targetedSites.set(url, []);
        }
        targetedSites.get(url).push(topSiteWithSuggestedLink);
      })
    });

    // setup timeout check for starting or ending campaigns
    if (nextTimeout) {
      this._setupCampaignTimeCheck(nextTimeout - Date.now());
    }

    // We might have run out of possible links to show
    let numLinks = possibleLinks.size;
    if (numLinks == 0) {
      return undefined;
    }

    let flattenedLinks = [...possibleLinks.values()];

    // Choose our suggested link at random
    let suggestedIndex = Math.floor(Math.random() * numLinks);
    let chosenSuggestedLink = flattenedLinks[suggestedIndex];

    // Add the suggested link to the front with some extra values
    this._callObservers("onLinkChanged", Object.assign({
      frecency: SUGGESTED_FRECENCY,

      // Choose the first site a user has visited as the target. In the future,
      // this should be the site with the highest frecency. However, we currently
      // store frecency by URL not by site.
      targetedSite: targetedSites.get(chosenSuggestedLink.url).length ?
        targetedSites.get(chosenSuggestedLink.url)[0] : null
    }, chosenSuggestedLink));
    return chosenSuggestedLink;
   },

  /**
   * Loads inadjacent sites
   * @return a promise resolved when lookup Set for sites is built
   */
  _loadInadjacentSites: function DirectoryLinksProvider_loadInadjacentSites() {
    return this._downloadJsonData(this._inadjacentSitesUrl).then(jsonString => {
      let jsonObject = {};
      try {
        jsonObject = JSON.parse(jsonString);
      }
      catch (e) {
        Cu.reportError(e);
      }

      this._inadjacentSites = new Set(jsonObject.domains);
    });
  },

  /**
   * Genegrates hash suitable for looking up inadjacent site
   * @param value to hsh
   * @return hased value, base64-ed
   */
  _generateHash: function DirectoryLinksProvider_generateHash(value) {
    let byteArr = gUnicodeConverter.convertToByteArray(value);
    gCryptoHash.init(gCryptoHash.MD5);
    gCryptoHash.update(byteArr, byteArr.length);
    return gCryptoHash.finish(true);
  },

  /**
   * Checks if link belongs to inadjacent domain
   * @param link to check
   * @return true for inadjacent domains, false otherwise
   */
  _isInadjacentLink: function DirectoryLinksProvider_isInadjacentLink(link) {
    let baseDomain = link.baseDomain || NewTabUtils.extractSite(link.url || "");
    if (!baseDomain) {
        return false;
    }
    // check if hashed domain is inadjacent
    return this._inadjacentSites.has(this._generateHash(baseDomain));
  },

  /**
   * Checks if new tab has inadjacent site
   * @param new tab links (or nothing, in which case NewTabUtils.links.getLinks() is called
   * @return true if new tab shows has inadjacent site
   */
  _checkForInadjacentSites: function DirectoryLinksProvider_checkForInadjacentSites(newTabLink) {
    let links = newTabLink || NewTabUtils.links.getLinks();
    for (let link of links.slice(0, MAX_VISIBLE_HISTORY_TILES)) {
      // check links against inadjacent list - specifically include ALL link types
      if (this._isInadjacentLink(link)) {
        return true;
      }
    }
    return false;
  },

  /**
   * Reads json file, parses its content, and returns resulting object
   * @param json file path
   * @param json object to return in case file read or parse fails
   * @return a promise resolved to a valid object or undefined upon error
   */
  _readJsonFile: Task.async(function* (filePath, nullObject) {
    let jsonObj;
    try {
      let binaryData = yield OS.File.read(filePath);
      let json = gTextDecoder.decode(binaryData);
      jsonObj = JSON.parse(json);
    }
    catch (e) {}
    return jsonObj || nullObject;
  }),

  /**
   * Loads frequency cap object from file and parses its content
   * @return a promise resolved upon load completion
   *         on error or non-exstent file _frequencyCaps is set to empty object
   */
  _readFrequencyCapFile: Task.async(function* () {
    // set _frequencyCaps object to file's content or empty object
    this._frequencyCaps = yield this._readJsonFile(this._frequencyCapFilePath, {});
  }),

  /**
   * Saves frequency cap object to file
   * @return a promise resolved upon file i/o completion
   */
  _writeFrequencyCapFile: function DirectoryLinksProvider_writeFrequencyCapFile() {
    let json = JSON.stringify(this._frequencyCaps || {});
    return OS.File.writeAtomic(this._frequencyCapFilePath, json, {tmpPath: this._frequencyCapFilePath + ".tmp"});
  },

  /**
   * Clears frequency cap object and writes empty json to file
   * @return a promise resolved upon file i/o completion
   */
  _clearFrequencyCap: function DirectoryLinksProvider_clearFrequencyCap() {
    this._frequencyCaps = {};
    return this._writeFrequencyCapFile();
  },

  /**
   * updates frequency cap configuration for a link
   */
  _updateFrequencyCapSettings: function DirectoryLinksProvider_updateFrequencyCapSettings(link) {
    let capsObject = this._frequencyCaps[link.url];
    if (!capsObject) {
      // create an object with empty counts
      capsObject = {
        dailyViews: 0,
        totalViews: 0,
        lastShownDate: 0,
      };
      this._frequencyCaps[link.url] = capsObject;
    }
    // set last updated timestamp
    capsObject.lastUpdated = Date.now();
    // check for link configuration
    if (link.frequency_caps) {
      capsObject.dailyCap = link.frequency_caps.daily || DEFAULT_DAILY_FREQUENCY_CAP;
      capsObject.totalCap = link.frequency_caps.total || DEFAULT_TOTAL_FREQUENCY_CAP;
    }
    else {
      // fallback to defaults
      capsObject.dailyCap = DEFAULT_DAILY_FREQUENCY_CAP;
      capsObject.totalCap = DEFAULT_TOTAL_FREQUENCY_CAP;
    }
  },

  /**
   * Prunes frequency cap objects for outdated links
   * @param timeDetla milliseconds
   *        all cap objects with lastUpdated less than (now() - timeDelta)
   *        will be removed. This is done to remove frequency cap objects
   *        for unused tile urls
   */
  _pruneFrequencyCapUrls: function DirectoryLinksProvider_pruneFrequencyCapUrls(timeDelta = DEFAULT_PRUNE_TIME_DELTA) {
    let timeThreshold = Date.now() - timeDelta;
    Object.keys(this._frequencyCaps).forEach(url => {
      // remove url if it is not ignorable and wasn't updated for a while
      if (!url.startsWith("ignore") && this._frequencyCaps[url].lastUpdated <= timeThreshold) {
        delete this._frequencyCaps[url];
      }
    });
  },

  /**
   * Checks if supplied timestamp happened today
   * @param timestamp in milliseconds
   * @return true if the timestamp was made today, false otherwise
   */
  _wasToday: function DirectoryLinksProvider_wasToday(timestamp) {
    let showOn = new Date(timestamp);
    let today = new Date();
    // call timestamps identical if both day and month are same
    return showOn.getDate() == today.getDate() &&
           showOn.getMonth() == today.getMonth() &&
           showOn.getYear() == today.getYear();
  },

  /**
   * adds some number of views for a url
   * @param url String url of the suggested link
   */
  _addFrequencyCapView: function DirectoryLinksProvider_addFrequencyCapView(url) {
    let capObject = this._frequencyCaps[url];
    // sanity check
    if (!capObject) {
      return;
    }

    // if the day is new: reset the daily counter and lastShownDate
    if (!this._wasToday(capObject.lastShownDate)) {
      capObject.dailyViews = 0;
      // update lastShownDate
      capObject.lastShownDate = Date.now();
    }

    // bump both daily and total counters
    capObject.totalViews++;
    capObject.dailyViews++;

    // if any of the caps is reached - update suggested tiles
    if (capObject.totalViews >= capObject.totalCap ||
        capObject.dailyViews >= capObject.dailyCap) {
      this._updateSuggestedTile();
    }
  },

  /**
   * Sets clicked flag for link url
   * @param url String url of the suggested link
   */
  _setFrequencyCapClick(url) {
    let capObject = this._frequencyCaps[url];
    // sanity check
    if (!capObject) {
      return;
    }
    capObject.clicked = true;
    // and update suggested tiles, since current tile became invalid
    this._updateSuggestedTile();
  },

  /**
   * Tests frequency cap limits for link url
   * @param url String url of the suggested link
   * @return true if link is viewable, false otherwise
   */
  _testFrequencyCapLimits: function DirectoryLinksProvider_testFrequencyCapLimits(url) {
    let capObject = this._frequencyCaps[url];
    // sanity check: if url is missing - do not show this tile
    if (!capObject) {
      return false;
    }

    // check for clicked set or total views reached
    if (capObject.clicked || capObject.totalViews >= capObject.totalCap) {
      return false;
    }

    // otherwise check if link is over daily views limit
    if (this._wasToday(capObject.lastShownDate) &&
        capObject.dailyViews >= capObject.dailyCap) {
      return false;
    }

    // we passed all cap tests: return true
    return true;
  },

  /**
   * Removes clicked flag from frequency cap entry for tile landing url
   * @param url String url of the suggested link
   * @return promise resolved upon disk write completion
   */
  _removeTileClick: function DirectoryLinksProvider_removeTileClick(url = "") {
    // remove trailing slash, to accomodate Places sending site urls ending with '/'
    let noTrailingSlashUrl = url.replace(/\/$/, "");
    let capObject = this._frequencyCaps[url] || this._frequencyCaps[noTrailingSlashUrl];
    // return resolved promise if capObject is not found
    if (!capObject) {
      return Promise.resolve();
    }
    // otherwise remove clicked flag
    delete capObject.clicked;
    return this._writeFrequencyCapFile();
  },

  /**
   * Removes all clicked flags from frequency cap object
   * @return promise resolved upon disk write completion
   */
  _removeAllTileClicks: function DirectoryLinksProvider_removeAllTileClicks() {
    Object.keys(this._frequencyCaps).forEach(url => {
      delete this._frequencyCaps[url].clicked;
    });
    return this._writeFrequencyCapFile();
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

  _removeObservers: function() {
    this._observers.clear();
  }
};
