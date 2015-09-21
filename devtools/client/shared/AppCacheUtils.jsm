/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * validateManifest() warns of the following errors:
 *  - No manifest specified in page
 *  - Manifest is not utf-8
 *  - Manifest mimetype not text/cache-manifest
 *  - Manifest does not begin with "CACHE MANIFEST"
 *  - Page modified since appcache last changed
 *  - Duplicate entries
 *  - Conflicting entries e.g. in both CACHE and NETWORK sections or in cache
 *    but blocked by FALLBACK namespace
 *  - Detect referenced files that are not available
 *  - Detect referenced files that have cache-control set to no-store
 *  - Wildcards used in a section other than NETWORK
 *  - Spaces in URI not replaced with %20
 *  - Completely invalid URIs
 *  - Too many dot dot slash operators
 *  - SETTINGS section is valid
 *  - Invalid section name
 *  - etc.
 */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

var { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});
var { Services }   = Cu.import("resource://gre/modules/Services.jsm", {});
var { LoadContextInfo } = Cu.import("resource://gre/modules/LoadContextInfo.jsm", {});
var { require } = Cu.import("resource://gre/modules/devtools/shared/Loader.jsm", {});
var promise = require("promise");

this.EXPORTED_SYMBOLS = ["AppCacheUtils"];

function AppCacheUtils(documentOrUri) {
  this._parseManifest = this._parseManifest.bind(this);

  if (documentOrUri) {
    if (typeof documentOrUri == "string") {
      this.uri = documentOrUri;
    }
    if (/HTMLDocument/.test(documentOrUri.toString())) {
      this.doc = documentOrUri;
    }
  }
}

AppCacheUtils.prototype = {
  get cachePath() {
    return "";
  },

  validateManifest: function ACU_validateManifest() {
    let deferred = promise.defer();
    this.errors = [];
    // Check for missing manifest.
    this._getManifestURI().then(manifestURI => {
      this.manifestURI = manifestURI;

      if (!this.manifestURI) {
        this._addError(0, "noManifest");
        deferred.resolve(this.errors);
      }

      this._getURIInfo(this.manifestURI).then(uriInfo => {
        this._parseManifest(uriInfo).then(() => {
          // Sort errors by line number.
          this.errors.sort(function(a, b) {
            return a.line - b.line;
          });
          deferred.resolve(this.errors);
        });
      });
    });

    return deferred.promise;
  },

  _parseManifest: function ACU__parseManifest(uriInfo) {
    let deferred = promise.defer();
    let manifestName = uriInfo.name;
    let manifestLastModified = new Date(uriInfo.responseHeaders["Last-Modified"]);

    if (uriInfo.charset.toLowerCase() != "utf-8") {
      this._addError(0, "notUTF8", uriInfo.charset);
    }

    if (uriInfo.mimeType != "text/cache-manifest") {
      this._addError(0, "badMimeType", uriInfo.mimeType);
    }

    let parser = new ManifestParser(uriInfo.text, this.manifestURI);
    let parsed = parser.parse();

    if (parsed.errors.length > 0) {
      this.errors.push.apply(this.errors, parsed.errors);
    }

    // Check for duplicate entries.
    let dupes = {};
    for (let parsedUri of parsed.uris) {
      dupes[parsedUri.uri] = dupes[parsedUri.uri] || [];
      dupes[parsedUri.uri].push({
        line: parsedUri.line,
        section: parsedUri.section,
        original: parsedUri.original
      });
    }
    for (let [uri, value] of Iterator(dupes)) {
      if (value.length > 1) {
        this._addError(0, "duplicateURI", uri, JSON.stringify(value));
      }
    }

    // Loop through network entries making sure that fallback and cache don't
    // contain uris starting with the network uri.
    for (let neturi of parsed.uris) {
      if (neturi.section == "NETWORK") {
        for (let parsedUri of parsed.uris) {
          if (parsedUri.section !== "NETWORK" &&
              parsedUri.uri.startsWith(neturi.uri)) {
            this._addError(neturi.line, "networkBlocksURI", neturi.line,
                           neturi.original, parsedUri.line, parsedUri.original,
                           parsedUri.section);
          }
        }
      }
    }

    // Loop through fallback entries making sure that fallback and cache don't
    // contain uris starting with the network uri.
    for (let fb of parsed.fallbacks) {
      for (let parsedUri of parsed.uris) {
        if (parsedUri.uri.startsWith(fb.namespace)) {
          this._addError(fb.line, "fallbackBlocksURI", fb.line,
                         fb.original, parsedUri.line, parsedUri.original,
                         parsedUri.section);
        }
      }
    }

    // Check that all resources exist and that their cach-control headers are
    // not set to no-store.
    let current = -1;
    for (let i = 0, len = parsed.uris.length; i < len; i++) {
      let parsedUri = parsed.uris[i];
      this._getURIInfo(parsedUri.uri).then(uriInfo => {
        current++;

        if (uriInfo.success) {
          // Check that the resource was not modified after the manifest was last
          // modified. If it was then the manifest file should be refreshed.
          let resourceLastModified =
            new Date(uriInfo.responseHeaders["Last-Modified"]);

          if (manifestLastModified < resourceLastModified) {
            this._addError(parsedUri.line, "fileChangedButNotManifest",
                           uriInfo.name, manifestName, parsedUri.line);
          }

          // If cache-control: no-store the file will not be added to the
          // appCache.
          if (uriInfo.nocache) {
            this._addError(parsedUri.line, "cacheControlNoStore",
                           parsedUri.original, parsedUri.line);
          }
        } else if (parsedUri.original !== "*") {
          this._addError(parsedUri.line, "notAvailable",
                         parsedUri.original, parsedUri.line);
        }

        if (current == len - 1) {
          deferred.resolve();
        }
      });
    }

    return deferred.promise;
  },

  _getURIInfo: function ACU__getURIInfo(uri) {
    let inputStream = Cc["@mozilla.org/scriptableinputstream;1"]
                        .createInstance(Ci.nsIScriptableInputStream);
    let deferred = promise.defer();
    let buffer = "";
    let channel = Services.io.newChannel2(uri,
                                          null,
                                          null,
                                          null,      // aLoadingNode
                                          Services.scriptSecurityManager.getSystemPrincipal(),
                                          null,      // aTriggeringPrincipal
                                          Ci.nsILoadInfo.SEC_NORMAL,
                                          Ci.nsIContentPolicy.TYPE_OTHER);

    // Avoid the cache:
    channel.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;
    channel.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;

    channel.asyncOpen({
      onStartRequest: function (request, context) {
        // This empty method is needed in order for onDataAvailable to be
        // called.
      },

      onDataAvailable: function (request, context, stream, offset, count) {
        request.QueryInterface(Ci.nsIHttpChannel);
        inputStream.init(stream);
        buffer = buffer.concat(inputStream.read(count));
      },

      onStopRequest: function onStartRequest(request, context, statusCode) {
        if (statusCode === 0) {
          request.QueryInterface(Ci.nsIHttpChannel);

          let result = {
            name: request.name,
            success: request.requestSucceeded,
            status: request.responseStatus + " - " + request.responseStatusText,
            charset: request.contentCharset || "utf-8",
            mimeType: request.contentType,
            contentLength: request.contentLength,
            nocache: request.isNoCacheResponse() || request.isNoStoreResponse(),
            prePath: request.URI.prePath + "/",
            text: buffer
          };

          result.requestHeaders = {};
          request.visitRequestHeaders(function(header, value) {
            result.requestHeaders[header] = value;
          });

          result.responseHeaders = {};
          request.visitResponseHeaders(function(header, value) {
            result.responseHeaders[header] = value;
          });

          deferred.resolve(result);
        } else {
          deferred.resolve({
            name: request.name,
            success: false
          });
        }
      }
    }, null);
    return deferred.promise;
  },

  listEntries: function ACU_show(searchTerm) {
    if (!Services.prefs.getBoolPref("browser.cache.disk.enable")) {
      throw new Error(l10n.GetStringFromName("cacheDisabled"));
    }

    let entries = [];

    let appCacheStorage = Services.cache2.appCacheStorage(LoadContextInfo.default, null);
    appCacheStorage.asyncVisitStorage({
      onCacheStorageInfo: function() {},

      onCacheEntryInfo: function(aURI, aIdEnhance, aDataSize, aFetchCount, aLastModifiedTime, aExpirationTime) {
        let lowerKey = aURI.asciiSpec.toLowerCase();

        if (searchTerm && lowerKey.indexOf(searchTerm.toLowerCase()) == -1) {
          return;
        }

        if (aIdEnhance) {
          aIdEnhance += ":";
        }

        let entry = {
          "deviceID": "offline",
          "key": aIdEnhance + aURI.asciiSpec,
          "fetchCount": aFetchCount,
          "lastFetched": null,
          "lastModified": new Date(aLastModifiedTime * 1000),
          "expirationTime": new Date(aExpirationTime * 1000),
          "dataSize": aDataSize
        };

        entries.push(entry);
        return true;
      }
    }, true);

    if (entries.length === 0) {
      throw new Error(l10n.GetStringFromName("noResults"));
    }
    return entries;
  },

  viewEntry: function ACU_viewEntry(key) {
    let wm = Cc["@mozilla.org/appshell/window-mediator;1"]
               .getService(Ci.nsIWindowMediator);
    let win = wm.getMostRecentWindow("navigator:browser");
    win.gBrowser.selectedTab = win.gBrowser.addTab(
      "about:cache-entry?storage=appcache&context=&eid=&uri=" + key);
  },

  clearAll: function ACU_clearAll() {
    if (!Services.prefs.getBoolPref("browser.cache.disk.enable")) {
      throw new Error(l10n.GetStringFromName("cacheDisabled"));
    }

    let appCacheStorage = Services.cache2.appCacheStorage(LoadContextInfo.default, null);
    appCacheStorage.asyncEvictStorage({
      onCacheEntryDoomed: function(result) {}
    });
  },

  _getManifestURI: function ACU__getManifestURI() {
    let deferred = promise.defer();

    let getURI = () => {
      let htmlNode = this.doc.querySelector("html[manifest]");
      if (htmlNode) {
        let pageUri = this.doc.location ? this.doc.location.href : this.uri;
        let origin = pageUri.substr(0, pageUri.lastIndexOf("/") + 1);
        let manifestURI = htmlNode.getAttribute("manifest");

        if (manifestURI.startsWith("/")) {
          manifestURI = manifestURI.substr(1);
        }

        return origin + manifestURI;
      }
    };

    if (this.doc) {
      let uri = getURI();
      return promise.resolve(uri);
    } else {
      this._getURIInfo(this.uri).then(uriInfo => {
        if (uriInfo.success) {
          let html = uriInfo.text;
          let parser = _DOMParser;
          this.doc = parser.parseFromString(html, "text/html");
          let uri = getURI();
          deferred.resolve(uri);
        } else {
          this.errors.push({
            line: 0,
            msg: l10n.GetStringFromName("invalidURI")
          });
        }
      });
    }
    return deferred.promise;
  },

  _addError: function ACU__addError(line, l10nString, ...params) {
    let msg;

    if (params) {
      msg = l10n.formatStringFromName(l10nString, params, params.length);
    } else {
      msg = l10n.GetStringFromName(l10nString);
    }

    this.errors.push({
      line: line,
      msg: msg
    });
  },
};

/**
 * We use our own custom parser because we need far more detailed information
 * than the system manifest parser provides.
 *
 * @param {String} manifestText
 *        The text content of the manifest file.
 * @param {String} manifestURI
 *        The URI of the manifest file. This is used in calculating the path of
 *        relative URIs.
 */
function ManifestParser(manifestText, manifestURI) {
  this.manifestText = manifestText;
  this.origin = manifestURI.substr(0, manifestURI.lastIndexOf("/") + 1)
                           .replace(" ", "%20");
}

ManifestParser.prototype = {
  parse: function OCIMP_parse() {
    let lines = this.manifestText.split(/\r?\n/);
    let fallbacks = this.fallbacks = [];
    let settings = this.settings = [];
    let errors = this.errors = [];
    let uris = this.uris = [];

    this.currSection = "CACHE";

    for (let i = 0; i < lines.length; i++) {
      let text = this.text = lines[i].trim();
      this.currentLine = i + 1;

      if (i === 0 && text !== "CACHE MANIFEST") {
        this._addError(1, "firstLineMustBeCacheManifest", 1);
      }

      // Ignore comments
      if (/^#/.test(text) || !text.length) {
        continue;
      }

      if (text == "CACHE MANIFEST") {
        if (this.currentLine != 1) {
          this._addError(this.currentLine, "cacheManifestOnlyFirstLine2",
                         this.currentLine);
        }
        continue;
      }

      if (this._maybeUpdateSectionName()) {
        continue;
      }

      switch (this.currSection) {
        case "CACHE":
        case "NETWORK":
          this.parseLine();
          break;
        case "FALLBACK":
          this.parseFallbackLine();
          break;
        case "SETTINGS":
          this.parseSettingsLine();
          break;
      }
    }

    return {
      uris: uris,
      fallbacks: fallbacks,
      settings: settings,
      errors: errors
    };
  },

  parseLine: function OCIMP_parseLine() {
    let text = this.text;

    if (text.indexOf("*") != -1) {
      if (this.currSection != "NETWORK" || text.length != 1) {
        this._addError(this.currentLine, "asteriskInWrongSection2",
                       this.currSection, this.currentLine);
        return;
      }
    }

    if (/\s/.test(text)) {
      this._addError(this.currentLine, "escapeSpaces", this.currentLine);
      text = text.replace(/\s/g, "%20");
    }

    if (text[0] == "/") {
      if (text.substr(0, 4) == "/../") {
        this._addError(this.currentLine, "slashDotDotSlashBad", this.currentLine);
      } else {
        this.uris.push(this._wrapURI(this.origin + text.substring(1)));
      }
    } else if (text.substr(0, 2) == "./") {
      this.uris.push(this._wrapURI(this.origin + text.substring(2)));
    } else if (text.substr(0, 4) == "http") {
      this.uris.push(this._wrapURI(text));
    } else {
      let origin = this.origin;
      let path = text;

      while (path.substr(0, 3) == "../" && /^https?:\/\/.*?\/.*?\//.test(origin)) {
        let trimIdx = origin.substr(0, origin.length - 1).lastIndexOf("/") + 1;
        origin = origin.substr(0, trimIdx);
        path = path.substr(3);
      }

      if (path.substr(0, 3) == "../") {
        this._addError(this.currentLine, "tooManyDotDotSlashes", this.currentLine);
        return;
      }

      if (/^https?:\/\//.test(path)) {
        this.uris.push(this._wrapURI(path));
        return;
      }
      this.uris.push(this._wrapURI(origin + path));
    }
  },

  parseFallbackLine: function OCIMP_parseFallbackLine() {
    let split = this.text.split(/\s+/);
    let origURI = this.text;

    if (split.length != 2) {
      this._addError(this.currentLine, "fallbackUseSpaces", this.currentLine);
      return;
    }

    let [ namespace, fallback ] = split;

    if (namespace.indexOf("*") != -1) {
      this._addError(this.currentLine, "fallbackAsterisk2", this.currentLine);
    }

    if (/\s/.test(namespace)) {
      this._addError(this.currentLine, "escapeSpaces", this.currentLine);
      namespace = namespace.replace(/\s/g, "%20");
    }

    if (namespace.substr(0, 4) == "/../") {
      this._addError(this.currentLine, "slashDotDotSlashBad", this.currentLine);
    }

    if (namespace.substr(0, 2) == "./") {
      namespace = this.origin + namespace.substring(2);
    }

    if (namespace.substr(0, 4) != "http") {
      let origin = this.origin;
      let path = namespace;

      while (path.substr(0, 3) == "../" && /^https?:\/\/.*?\/.*?\//.test(origin)) {
        let trimIdx = origin.substr(0, origin.length - 1).lastIndexOf("/") + 1;
        origin = origin.substr(0, trimIdx);
        path = path.substr(3);
      }

      if (path.substr(0, 3) == "../") {
        this._addError(this.currentLine, "tooManyDotDotSlashes", this.currentLine);
      }

      if (/^https?:\/\//.test(path)) {
        namespace = path;
      } else {
        if (path[0] == "/") {
          path = path.substring(1);
        }
        namespace = origin + path;
      }
    }

    this.text = fallback;
    this.parseLine();

    this.fallbacks.push({
      line: this.currentLine,
      original: origURI,
      namespace: namespace,
      fallback: fallback
    });
  },

  parseSettingsLine: function OCIMP_parseSettingsLine() {
    let text = this.text;

    if (this.settings.length == 1 || !/prefer-online|fast/.test(text)) {
      this._addError(this.currentLine, "settingsBadValue", this.currentLine);
      return;
    }

    switch (text) {
      case "prefer-online":
        this.settings.push(this._wrapURI(text));
        break;
      case "fast":
        this.settings.push(this._wrapURI(text));
        break;
    }
  },

  _wrapURI: function OCIMP__wrapURI(uri) {
    return {
      section: this.currSection,
      line: this.currentLine,
      uri: uri,
      original: this.text
    };
  },

  _addError: function OCIMP__addError(line, l10nString, ...params) {
    let msg;

    if (params) {
      msg = l10n.formatStringFromName(l10nString, params, params.length);
    } else {
      msg = l10n.GetStringFromName(l10nString);
    }

    this.errors.push({
      line: line,
      msg: msg
    });
  },

  _maybeUpdateSectionName: function OCIMP__maybeUpdateSectionName() {
    let text = this.text;

    if (text == text.toUpperCase() && text.charAt(text.length - 1) == ":") {
      text = text.substr(0, text.length - 1);

      switch (text) {
        case "CACHE":
        case "NETWORK":
        case "FALLBACK":
        case "SETTINGS":
          this.currSection = text;
          return true;
        default:
          this._addError(this.currentLine,
                         "invalidSectionName", text, this.currentLine);
          return false;
      }
    }
  },
};

XPCOMUtils.defineLazyGetter(this, "l10n", function() Services.strings
  .createBundle("chrome://browser/locale/devtools/appcacheutils.properties"));

XPCOMUtils.defineLazyGetter(this, "appcacheservice", function() {
  return Cc["@mozilla.org/network/application-cache-service;1"]
           .getService(Ci.nsIApplicationCacheService);

});

XPCOMUtils.defineLazyGetter(this, "_DOMParser", function() {
  return Cc["@mozilla.org/xmlextras/domparser;1"].createInstance(Ci.nsIDOMParser);
});
