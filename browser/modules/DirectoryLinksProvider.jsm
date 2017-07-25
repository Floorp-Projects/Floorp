/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["DirectoryLinksProvider"];

const Cu = Components.utils;
Cu.importGlobalProperties(["XMLHttpRequest"]);

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "OS",
  "resource://gre/modules/osfile.jsm")
XPCOMUtils.defineLazyModuleGetter(this, "PromiseUtils",
  "resource://gre/modules/PromiseUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UpdateUtils",
  "resource://gre/modules/UpdateUtils.jsm");
XPCOMUtils.defineLazyGetter(this, "gTextDecoder", () => {
  return new TextDecoder();
});


// The filename where directory links are stored locally
const DIRECTORY_LINKS_FILE = "directoryLinks.json";
const DIRECTORY_LINKS_TYPE = "application/json";

// The preference that tells where to obtain directory links
const PREF_DIRECTORY_SOURCE = "browser.newtabpage.directory.source";

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

  get _observedPrefs() {
    return Object.freeze({
      enhanced: PREF_NEWTAB_ENHANCED,
      linksURL: PREF_DIRECTORY_SOURCE,
    });
  },

  get _linksURL() {
    if (!this.__linksURL) {
      try {
        this.__linksURL = Services.prefs.getCharPref(this._observedPrefs.linksURL);
        this.__linksURLModified = Services.prefs.prefHasUserValue(this._observedPrefs.linksURL);
      } catch (e) {
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
    return Services.locale.getRequestedLocale() || "en-US";
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
      } catch (ex) {}
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
          this._fetchAndCacheLinksIfNecessary(true);
          break;
      }
    } else if (aTopic === "intl:requested-locales-changed") {
      this._fetchAndCacheLinksIfNecessary(true);
    }
  },

  _addPrefsObserver: function DirectoryLinksProvider_addObserver() {
    for (let pref in this._observedPrefs) {
      let prefName = this._observedPrefs[pref];
      Services.prefs.addObserver(prefName, this);
    }
  },

  _removePrefsObserver: function DirectoryLinksProvider_removeObserver() {
    for (let pref in this._observedPrefs) {
      let prefName = this._observedPrefs[pref];
      Services.prefs.removeObserver(prefName, this);
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
    return new Promise((resolve, reject) => {
      let xmlHttp = this._newXHR();

      xmlHttp.onload = function(aResponse) {
        let json = this.responseText;
        if (this.status && this.status != 200) {
          json = "{}";
        }
        resolve(json);
      };

      xmlHttp.onerror = function(e) {
        reject("Fetching " + uri + " results in error code: " + e.target.status);
      };

      try {
        xmlHttp.open("GET", uri);
        // Override the type so XHR doesn't complain about not well-formed XML
        xmlHttp.overrideMimeType(DIRECTORY_LINKS_TYPE);
        // Set the appropriate request type for servers that require correct types
        xmlHttp.setRequestHeader("Content-Type", DIRECTORY_LINKS_TYPE);
        xmlHttp.send();
      } catch (e) {
        reject("Error fetching " + uri);
        Cu.reportError(e);
      }
    });
  },

  /**
   * Downloads directory links if needed
   * @return promise resolved immediately if no download needed, or upon completion
   */
  _fetchAndCacheLinksIfNecessary: function DirectoryLinksProvider_fetchAndCacheLinksIfNecessary(forceDownload = false) {
    if (this._downloadDeferred) {
      // fetching links already - just return the promise
      return this._downloadDeferred.promise;
    }

    if (forceDownload || this._needsDownload) {
      this._downloadDeferred = PromiseUtils.defer();
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
  get _needsDownload() {
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
    let emptyOutput = {directory: []};
    return OS.File.read(this._directoryFilePath).then(binaryData => {
      let output;
      try {
        let json = gTextDecoder.decode(binaryData);
        let linksObj = JSON.parse(json);
        output = {directory: linksObj.directory || []};
      } catch (e) {
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
   * Get the enhanced link object for a link (whether history or directory)
   */
  getEnhancedLink: function DirectoryLinksProvider_getEnhancedLink(link) {
    // Use the provided link if it's already enhanced
    return link.enhancedImageURI && link;
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
      let uri = Services.io.newURI(url);
      scheme = uri.scheme;

      // URIs without base domains will be allowed
      base = Services.eTLD.getBaseDomain(uri);
    } catch (ex) {}
    // Require a scheme match and the base only if desired
    return allowed.has(scheme) && (!checkBase || ALLOWED_URL_BASE.has(base));
  },

  /**
   * Gets the current set of directory links.
   * @param aCallback The function that the array of links is passed to.
   */
  getLinks: function DirectoryLinksProvider_getLinks(aCallback) {
    this._readDirectoryLinksFile().then(rawLinks => {
      // Only check base domain for images when using the default pref
      let checkBase = !this.__linksURLModified;
      let validityFilter = link => {
        // Make sure the link url is allowed and images too if they exist
        return this.isURLAllowed(link.url, ALLOWED_LINK_SCHEMES, false) &&
               (!link.imageURI ||
                this.isURLAllowed(link.imageURI, ALLOWED_IMAGE_SCHEMES, checkBase)) &&
               (!link.enhancedImageURI ||
                this.isURLAllowed(link.enhancedImageURI, ALLOWED_IMAGE_SCHEMES, checkBase));
      };

      let links = rawLinks.directory.filter(validityFilter).map((link, position) => {
        link.lastVisitDate = rawLinks.directory.length - position;
        link.frecency = DIRECTORY_FRECENCY;
        return link;
      });

      return links;
    }).catch(ex => {
      Cu.reportError(ex);
      return [];
    }).then(links => {
      aCallback(links);
    });
  },

  init: function DirectoryLinksProvider_init() {
    this._setDefaultEnhanced();
    this._addPrefsObserver();
    Services.obs.addObserver(this, "intl:requested-locales-changed");
    // setup directory file path and last download timestamp
    this._directoryFilePath = OS.Path.join(OS.Constants.Path.localProfileDir, DIRECTORY_LINKS_FILE);
    this._lastDownloadMS = 0;

    return (async () => {
      // get the last modified time of the links file if it exists
      let doesFileExists = await OS.File.exists(this._directoryFilePath);
      if (doesFileExists) {
        let fileInfo = await OS.File.stat(this._directoryFilePath);
        this._lastDownloadMS = Date.parse(fileInfo.lastModificationDate);
      }
      // fetch directory on startup without force
      await this._fetchAndCacheLinksIfNecessary();
    })();
  },

  /**
   * Return the object to its pre-init state
   */
  reset: function DirectoryLinksProvider_reset() {
    delete this.__linksURL;
    this._removePrefsObserver();
    this._removeObservers();
    Services.obs.removeObserver(this, "intl:requested-locales-changed");
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

  _removeObservers() {
    this._observers.clear();
  }
};
