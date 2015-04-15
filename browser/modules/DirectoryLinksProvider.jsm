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
XPCOMUtils.defineLazyModuleGetter(this, "UpdateChannel",
  "resource://gre/modules/UpdateChannel.jsm");
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

// The frecency of a suggested link
const SUGGESTED_FRECENCY = Infinity;

// Default number of times to show a link
const DEFAULT_FREQUENCY_CAP = 5;

// The min number of visible (not blocked) history tiles to have before showing suggested tiles
const MIN_VISIBLE_HISTORY_TILES = 8;

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
   * A mapping from site to remaining number of views
   */
  _frequencyCaps: new Map(),

  /**
   * A mapping from site to a list of suggested link objects
   */
  _suggestedLinks: new Map(),

  /**
   * A set of top sites that we can provide suggested links for
   */
  _topSitesWithSuggestedLinks: new Set(),

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

        // Temporarily override the default for en-US until new endpoint is live
        if (this.locale == "en-US" && !Services.prefs.prefHasUserValue(this._observedPrefs["linksURL"])) {
          this.__linksURL = "data:text/plain;base64,ewogICAgImRpcmVjdG9yeSI6IFsKICAgICAgICB7CiAgICAgICAgICAgICJiZ0NvbG9yIjogIiIsCiAgICAgICAgICAgICJkaXJlY3RvcnlJZCI6IDQ5OCwKICAgICAgICAgICAgImVuaGFuY2VkSW1hZ2VVUkkiOiAiaHR0cHM6Ly9kdGV4NGt2YnBwb3Z0LmNsb3VkZnJvbnQubmV0L2ltYWdlcy9kMTFiYTBiMzA5NWJiMTlkODA5MmNkMjliZTljYmI5ZTE5NzY3MWVhLjI4MDg4LnBuZyIsCiAgICAgICAgICAgICJpbWFnZVVSSSI6ICJodHRwczovL2R0ZXg0a3ZicHBvdnQuY2xvdWRmcm9udC5uZXQvaW1hZ2VzLzEzMzJhNjhiYWRmMTFlM2Y3ZjY5YmY3MzY0ZTc5YzBhN2UyNzUzYmMuNTMxNi5wbmciLAogICAgICAgICAgICAidGl0bGUiOiAiTW96aWxsYSBDb21tdW5pdHkiLAogICAgICAgICAgICAidHlwZSI6ICJhZmZpbGlhdGUiLAogICAgICAgICAgICAidXJsIjogImh0dHA6Ly9jb250cmlidXRlLm1vemlsbGEub3JnLyIKICAgICAgICB9LAogICAgICAgIHsKICAgICAgICAgICAgImJnQ29sb3IiOiAiIiwKICAgICAgICAgICAgImRpcmVjdG9yeUlkIjogNTAwLAogICAgICAgICAgICAiZW5oYW5jZWRJbWFnZVVSSSI6ICJodHRwczovL2R0ZXg0a3ZicHBvdnQuY2xvdWRmcm9udC5uZXQvaW1hZ2VzL2NjNjM3NzRiN2E5YWFlMDJmZTM2YmM1Y2FmOTBjMWUyNWU2NmEyYmMuMTM3OTEucG5nIiwKICAgICAgICAgICAgImltYWdlVVJJIjogImh0dHBzOi8vZHRleDRrdmJwcG92dC5jbG91ZGZyb250Lm5ldC9pbWFnZXMvZTgyMmNkNDYyOGM1MTYyMzEzZjQ5ZjVkNDU1NmY4YWFmZGYzODc1MC4xMTUxMy5wbmciLAogICAgICAgICAgICAidGl0bGUiOiAiTW96aWxsYSBNYW5pZmVzdG8iLAogICAgICAgICAgICAidHlwZSI6ICJhZmZpbGlhdGUiLAogICAgICAgICAgICAidXJsIjogImh0dHBzOi8vd3d3Lm1vemlsbGEub3JnL2Fib3V0L21hbmlmZXN0by8iCiAgICAgICAgfSwKICAgICAgICB7CiAgICAgICAgICAgICJiZ0NvbG9yIjogIiIsCiAgICAgICAgICAgICJkaXJlY3RvcnlJZCI6IDUwMiwKICAgICAgICAgICAgImVuaGFuY2VkSW1hZ2VVUkkiOiAiaHR0cHM6Ly9kdGV4NGt2YnBwb3Z0LmNsb3VkZnJvbnQubmV0L2ltYWdlcy80MGU1NjMwNDA1ZDUwMzFjYTczMzkzYmQ3YmMwMDY0MTU2ZjJjYzgyLjEwOTg0LnBuZyIsCiAgICAgICAgICAgICJpbWFnZVVSSSI6ICJodHRwczovL2R0ZXg0a3ZicHBvdnQuY2xvdWRmcm9udC5uZXQvaW1hZ2VzLzQ5MGQ0MmQxZjlhNzZjMDc3Mzk2MjZkMWI4YTU2OTE2OWFlYzhmYmUuMTEwMzkucG5nIiwKICAgICAgICAgICAgInRpdGxlIjogIkN1c3RvbWl6ZSBGaXJlZm94IiwKICAgICAgICAgICAgInR5cGUiOiAiYWZmaWxpYXRlIiwKICAgICAgICAgICAgInVybCI6ICJodHRwOi8vZmFzdGVzdGZpcmVmb3guY29tL2ZpcmVmb3gvZGVza3RvcC9jdXN0b21pemUvIgogICAgICAgIH0sCiAgICAgICAgewogICAgICAgICAgICAiYmdDb2xvciI6ICIiLAogICAgICAgICAgICAiZGlyZWN0b3J5SWQiOiA1MDQsCiAgICAgICAgICAgICJlbmhhbmNlZEltYWdlVVJJIjogImh0dHBzOi8vZHRleDRrdmJwcG92dC5jbG91ZGZyb250Lm5ldC9pbWFnZXMvODc3ZjFjNTYxZTczNWY3YjlmNDE5ZmY5YWM3OWViOGM3NDgxMTE5ZC4xNjc0NC5wbmciLAogICAgICAgICAgICAiaW1hZ2VVUkkiOiAiaHR0cHM6Ly9kdGV4NGt2YnBwb3Z0LmNsb3VkZnJvbnQubmV0L2ltYWdlcy8yNWM5ZmJiMDczMDhiODRkMTYwZmMxYjc5NTkzNjRhMmMxOGY5M2I5LjY0MDQucG5nIiwKICAgICAgICAgICAgInRpdGxlIjogIkZpcmVmb3ggTWFya2V0cGxhY2UiLAogICAgICAgICAgICAidHlwZSI6ICJhZmZpbGlhdGUiLAogICAgICAgICAgICAidXJsIjogImh0dHBzOi8vbWFya2V0cGxhY2UuZmlyZWZveC5jb20vIgogICAgICAgIH0sCiAgICAgICAgewogICAgICAgICAgICAiYmdDb2xvciI6ICIjM2ZiNThlIiwKICAgICAgICAgICAgImRpcmVjdG9yeUlkIjogNTA1LAogICAgICAgICAgICAiZW5oYW5jZWRJbWFnZVVSSSI6ICJodHRwczovL2R0ZXg0a3ZicHBvdnQuY2xvdWRmcm9udC5uZXQvaW1hZ2VzLzcyMDEyMWU3NDYyZDhjNzg2M2I0ZGQ4ZmE3YjVjMTA4OWI1ZjVmYjIuMzM4NjIucG5nIiwKICAgICAgICAgICAgImltYWdlVVJJIjogImh0dHBzOi8vZHRleDRrdmJwcG92dC5jbG91ZGZyb250Lm5ldC9pbWFnZXMvMGU2MDMxNjc1YTljNDkxZGQwYzY1ZTljNjdjZmJmNTRhNTg4MGYxNy4yMjk1LnN2ZyIsCiAgICAgICAgICAgICJ0aXRsZSI6ICJNb3ppbGxhIFdlYm1ha2VyIiwKICAgICAgICAgICAgInR5cGUiOiAiYWZmaWxpYXRlIiwKICAgICAgICAgICAgInVybCI6ICJodHRwczovL3dlYm1ha2VyLm9yZy8%2FdXRtX3NvdXJjZT1kaXJlY3RvcnktdGlsZXMmdXRtX21lZGl1bT1maXJlZm94LWJyb3dzZXIiCiAgICAgICAgfSwKICAgICAgICB7CiAgICAgICAgICAgICJiZ0NvbG9yIjogIiIsCiAgICAgICAgICAgICJkaXJlY3RvcnlJZCI6IDUwNiwKICAgICAgICAgICAgImVuaGFuY2VkSW1hZ2VVUkkiOiAiaHR0cHM6Ly9kdGV4NGt2YnBwb3Z0LmNsb3VkZnJvbnQubmV0L2ltYWdlcy9kOTcxY2JhZmEwMzA5YTIwMWU1MThhY2RhYzRmMWVlNGRhYmM3ZWFhLjE1MTA5LnBuZyIsCiAgICAgICAgICAgICJpbWFnZVVSSSI6ICJodHRwczovL2R0ZXg0a3ZicHBvdnQuY2xvdWRmcm9udC5uZXQvaW1hZ2VzL2I0YWRjNThkZDNjMDJkYTM1NTEwNDk3N2I5MTAyNTUwNjBjZmQ2ZDguMTAzNTAucG5nIiwKICAgICAgICAgICAgInRpdGxlIjogIkZpcmVmb3ggU3luYyIsCiAgICAgICAgICAgICJ0eXBlIjogImFmZmlsaWF0ZSIsCiAgICAgICAgICAgICJ1cmwiOiAiaHR0cDovL21vemlsbGEtZXVyb3BlLm9yZy9maXJlZm94L3N5bmMiCiAgICAgICAgfSwKICAgICAgICB7CiAgICAgICAgICAgICJiZ0NvbG9yIjogIiIsCiAgICAgICAgICAgICJkaXJlY3RvcnlJZCI6IDUwNywKICAgICAgICAgICAgImVuaGFuY2VkSW1hZ2VVUkkiOiAiaHR0cHM6Ly9kdGV4NGt2YnBwb3Z0LmNsb3VkZnJvbnQubmV0L2ltYWdlcy8yMmZiODU2Y2Q1ODM2NTg1NWViNzI1YjE1NjVmMDhhNzI0NjRlMDM5LjE4NzE3LnBuZyIsCiAgICAgICAgICAgICJpbWFnZVVSSSI6ICJodHRwczovL2R0ZXg0a3ZicHBvdnQuY2xvdWRmcm9udC5uZXQvaW1hZ2VzLzA2OGUwY2NiZDg3MDFhMjhlMmYwNzhjNjQwZWUwNzJiOWExNmUyZTEuMTI0OTAucG5nIiwKICAgICAgICAgICAgInRpdGxlIjogIlByaXZhY3kgUHJpbmNpcGxlcyIsCiAgICAgICAgICAgICJ0eXBlIjogImFmZmlsaWF0ZSIsCiAgICAgICAgICAgICJ1cmwiOiAiaHR0cDovL2V1cm9wZS5tb3ppbGxhLm9yZy9wcml2YWN5L3lvdSIKICAgICAgICB9CiAgICBdLAogICAgInN1Z2dlc3RlZCI6IFsKICAgICAgICB7CiAgICAgICAgICAgICJiZ0NvbG9yIjogIiNjYWUxZjQiLAogICAgICAgICAgICAiZGlyZWN0b3J5SWQiOiA3MDIsCiAgICAgICAgICAgICJmcmVjZW50X3NpdGVzIjogWwogICAgICAgICAgICAgICAgImFkZG9ucy5tb3ppbGxhLm9yZyIsCiAgICAgICAgICAgICAgICAiYWlyLm1vemlsbGEub3JnIiwKICAgICAgICAgICAgICAgICJibG9nLm1vemlsbGEub3JnIiwKICAgICAgICAgICAgICAgICJidWd6aWxsYS5tb3ppbGxhLm9yZyIsCiAgICAgICAgICAgICAgICAiZGV2ZWxvcGVyLm1vemlsbGEub3JnIiwKICAgICAgICAgICAgICAgICJldGhlcnBhZC5tb3ppbGxhLm9yZyIsCiAgICAgICAgICAgICAgICAiaGFja3MubW96aWxsYS5vcmciLAogICAgICAgICAgICAgICAgImhnLm1vemlsbGEub3JnIiwKICAgICAgICAgICAgICAgICJtb3ppbGxhLm9yZyIsCiAgICAgICAgICAgICAgICAicGxhbmV0Lm1vemlsbGEub3JnIiwKICAgICAgICAgICAgICAgICJxdWFsaXR5Lm1vemlsbGEub3JnIiwKICAgICAgICAgICAgICAgICJzdXBwb3J0Lm1vemlsbGEub3JnIiwKICAgICAgICAgICAgICAgICJzdXBwb3J0Lm1vemlsbGEub3JnIiwKICAgICAgICAgICAgICAgICJ0cmVlaGVyZGVyLm1vemlsbGEub3JnIiwKICAgICAgICAgICAgICAgICJ3aWtpLm1vemlsbGEub3JnIgogICAgICAgICAgICBdLAogICAgICAgICAgICAiaW1hZ2VVUkkiOiAiaHR0cHM6Ly9kdGV4NGt2YnBwb3Z0LmNsb3VkZnJvbnQubmV0L2ltYWdlcy85ZWUyYjI2NTY3OGYyNzc1ZGUyZTRiZjY4MGRmNjAwYjUwMmU2MDM4LjM4NzUucG5nIiwKICAgICAgICAgICAgInRpdGxlIjogIlRoYW5rcyBmb3IgdGVzdGluZyEiLAogICAgICAgICAgICAidHlwZSI6ICJhZmZpbGlhdGUiLAogICAgICAgICAgICAidXJsIjogImh0dHBzOi8vd3d3Lm1vemlsbGEuY29tL2ZpcmVmb3gvdGlsZXMiCiAgICAgICAgfQogICAgXQp9Cg%3D%3D";
        }
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

  _cacheSuggestedLinks: function(link) {
    if (!link.frecent_sites || "sponsored" == link.type) {
      // Don't cache links that don't have the expected 'frecent_sites' or are sponsored.
      return;
    }
    for (let suggestedSite of link.frecent_sites) {
      let suggestedMap = this._suggestedLinks.get(suggestedSite) || new Map();
      suggestedMap.set(link.url, link);
      this._suggestedLinks.set(suggestedSite, suggestedMap);
    }
  },

  _fetchAndCacheLinks: function DirectoryLinksProvider_fetchAndCacheLinks(uri) {
    // Replace with the same display locale used for selecting links data
    uri = uri.replace("%LOCALE%", this.locale);
    uri = uri.replace("%CHANNEL%", UpdateChannel.get());

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
   * Report some action on a newtab page (view, click)
   * @param sites Array of sites shown on newtab page
   * @param action String of the behavior to report
   * @param triggeringSiteIndex optional Int index of the site triggering action
   * @return download promise
   */
  reportSitesAction: function DirectoryLinksProvider_reportSitesAction(sites, action, triggeringSiteIndex) {
    // Check if the suggested tile was shown
    if (action == "view") {
      sites.slice(0, triggeringSiteIndex + 1).forEach(site => {
        let {targetedSite, url} = site.link;
        if (targetedSite) {
          this._decreaseFrequencyCap(url, 1);
        }
      });
    }
    // Use up all views if the user clicked on a frequency capped tile
    else if (action == "click") {
      let {targetedSite, url} = sites[triggeringSiteIndex].link;
      if (targetedSite) {
        this._decreaseFrequencyCap(url, DEFAULT_FREQUENCY_CAP);
      }
    }

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
    return link.enhancedImageURI && link ? link :
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
      // Reset the cache of suggested tiles and enhanced images for this new set of links
      this._enhancedLinks.clear();
      this._frequencyCaps.clear();
      this._suggestedLinks.clear();

      let validityFilter = function(link) {
        // Make sure the link url is allowed and images too if they exist
        return this.isURLAllowed(link.url, ALLOWED_LINK_SCHEMES) &&
               this.isURLAllowed(link.imageURI, ALLOWED_IMAGE_SCHEMES) &&
               this.isURLAllowed(link.enhancedImageURI, ALLOWED_IMAGE_SCHEMES);
      }.bind(this);

      rawLinks.suggested.filter(validityFilter).forEach((link, position) => {
        link.lastVisitDate = rawLinks.suggested.length - position;

        // We cache suggested tiles here but do not push any of them in the links list yet.
        // The decision for which suggested tile to include will be made separately.
        this._cacheSuggestedLinks(link);
        this._frequencyCaps.set(link.url, DEFAULT_FREQUENCY_CAP);
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
    NewTabUtils.links.addObserver(this);

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

  /**
   * Record for a url that some number of views have been used
   * @param url String url of the suggested link
   * @param amount Number of equivalent views to decrease
   */
  _decreaseFrequencyCap(url, amount) {
    let remainingViews = this._frequencyCaps.get(url) - amount;
    this._frequencyCaps.set(url, remainingViews);

    // Reached the number of views, so pick a new one.
    if (remainingViews <= 0) {
      this._updateSuggestedTile();
    }
  },

  _getCurrentTopSiteCount: function() {
    let visibleTopSiteCount = 0;
    for (let link of NewTabUtils.links.getLinks().slice(0, MIN_VISIBLE_HISTORY_TILES)) {
      if (link && (link.type == "history" || link.type == "enhanced")) {
        visibleTopSiteCount++;
      }
    }
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
      return;
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

    if (this._topSitesWithSuggestedLinks.size == 0 || !this._shouldUpdateSuggestedTile()) {
      // There are no potential suggested links we can show or not
      // enough history for a suggested tile.
      return;
    }

    // Create a flat list of all possible links we can show as suggested.
    // Note that many top sites may map to the same suggested links, but we only
    // want to count each suggested link once (based on url), thus possibleLinks is a map
    // from url to suggestedLink. Thus, each link has an equal chance of being chosen at
    // random from flattenedLinks if it appears only once.
    let possibleLinks = new Map();
    let targetedSites = new Map();
    this._topSitesWithSuggestedLinks.forEach(topSiteWithSuggestedLink => {
      let suggestedLinksMap = this._suggestedLinks.get(topSiteWithSuggestedLink);
      suggestedLinksMap.forEach((suggestedLink, url) => {
        // Skip this link if we've shown it too many times already
        if (this._frequencyCaps.get(url) <= 0) {
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

    // We might have run out of possible links to show
    let numLinks = possibleLinks.size;
    if (numLinks == 0) {
      return;
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
