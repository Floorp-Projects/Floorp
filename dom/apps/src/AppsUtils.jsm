/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// Shared code for AppsServiceChild.jsm, Webapps.jsm and Webapps.js

let EXPORTED_SYMBOLS = ["AppsUtils", "ManifestHelper"];

function debug(s) {
  //dump("-*- AppsUtils.jsm: " + s + "\n");
}

let AppsUtils = {
  // Clones a app, without the manifest.
  cloneAppObject: function cloneAppObject(aApp) {
    return {
      name: aApp.name,
      installOrigin: aApp.installOrigin,
      origin: aApp.origin,
      receipts: aApp.receipts ? JSON.parse(JSON.stringify(aApp.receipts)) : null,
      installTime: aApp.installTime,
      manifestURL: aApp.manifestURL,
      appStatus: aApp.appStatus,
      removable: aApp.removable,
      localId: aApp.localId,
      progress: aApp.progress || 0.0,
      installState: aApp.installState || "installed",
      downloadAvailable: aApp.downloadAvailable,
      downloading: aApp.downloading,
      readyToApplyDownload: aApp.readyToApplyDownload,
      downloadSize: aApp.downloadSize || 0,
      lastUpdateCheck: aApp.lastUpdateCheck,
      etag: aApp.etag
    };
  },

  cloneAsMozIApplication: function cloneAsMozIApplication(aApp) {
    let res = this.cloneAppObject(aApp);
    res.hasPermission = function(aPermission) {
      let uri = Services.io.newURI(this.origin, null, null);
      let secMan = Cc["@mozilla.org/scriptsecuritymanager;1"]
                     .getService(Ci.nsIScriptSecurityManager);
      // This helper checks an URI inside |aApp|'s origin and part of |aApp| has a
      // specific permission. It is not checking if browsers inside |aApp| have such
      // permission.
      let principal = secMan.getAppCodebasePrincipal(uri, aApp.localId,
                                                     /*mozbrowser*/false);
      let perm = Services.perms.testExactPermissionFromPrincipal(principal,
                                                                 aPermission);
      return (perm === Ci.nsIPermissionManager.ALLOW_ACTION);
    };
    res.QueryInterface = XPCOMUtils.generateQI([Ci.mozIDOMApplication,
                                                Ci.mozIApplication]);
    return res;
  },

  getAppByManifestURL: function getAppByManifestURL(aApps, aManifestURL) {
    debug("getAppByManifestURL " + aManifestURL);
    // This could be O(1) if |webapps| was a dictionary indexed on manifestURL
    // which should be the unique app identifier.
    // It's currently O(n).
    for (let id in aApps) {
      let app = aApps[id];
      if (app.manifestURL == aManifestURL) {
        return this.cloneAsMozIApplication(app);
      }
    }

    return null;
  },

  getAppLocalIdByManifestURL: function getAppLocalIdByManifestURL(aApps, aManifestURL) {
    debug("getAppLocalIdByManifestURL " + aManifestURL);
    for (let id in aApps) {
      if (aApps[id].manifestURL == aManifestURL) {
        return aApps[id].localId;
      }
    }

    return Ci.nsIScriptSecurityManager.NO_APP_ID;
  },

  getAppByLocalId: function getAppByLocalId(aApps, aLocalId) {
    debug("getAppByLocalId " + aLocalId);
    for (let id in aApps) {
      let app = aApps[id];
      if (app.localId == aLocalId) {
        return this.cloneAsMozIApplication(app);
      }
    }

    return null;
  },

  getManifestURLByLocalId: function getManifestURLByLocalId(aApps, aLocalId) {
    debug("getManifestURLByLocalId " + aLocalId);
    for (let id in aApps) {
      let app = aApps[id];
      if (app.localId == aLocalId) {
        return app.manifestURL;
      }
    }

    return "";
  },

  getAppFromObserverMessage: function(aApps, aMessage) {
    let data = JSON.parse(aMessage);

    for (let id in aApps) {
      let app = aApps[id];
      if (app.origin != data.origin) {
        continue;
      }

      return this.cloneAsMozIApplication(app);
    }

    return null;
  },

  /**
   * from https://developer.mozilla.org/en/OpenWebApps/The_Manifest
   * only the name property is mandatory
   */
  checkManifest: function(aManifest, aInstallOrigin) {
    if (aManifest.name == undefined)
      return false;

    function cbCheckAllowedOrigin(aOrigin) {
      return aOrigin == "*" || aOrigin == aInstallOrigin;
    }

    if (aManifest.installs_allowed_from && !aManifest.installs_allowed_from.some(cbCheckAllowedOrigin))
      return false;

    function isAbsolute(uri) {
      try {
        Services.io.newURI(uri, null, null);
      } catch (e if e.result == Cr.NS_ERROR_MALFORMED_URI) {
        return false;
      }
      return true;
    }

    // launch_path and entry_points launch paths can't be absolute
    if (aManifest.launch_path && isAbsolute(aManifest.launch_path))
      return false;

    function checkAbsoluteEntryPoints(entryPoints) {
      for (let name in entryPoints) {
        if (entryPoints[name].launch_path && isAbsolute(entryPoints[name].launch_path)) {
          return true;
        }
      }
      return false;
    }

    if (checkAbsoluteEntryPoints(aManifest.entry_points))
      return false;

    for (let localeName in aManifest.locales) {
      if (checkAbsoluteEntryPoints(aManifest.locales[localeName].entry_points)) {
        return false;
      }
    }

    return true;
  },

  /**
 * Determine the type of app (app, privileged, certified)
 * that is installed by the manifest
 * @param object aManifest
 * @returns integer
 **/
  getAppManifestStatus: function getAppManifestStatus(aManifest) {
    let type = aManifest.type || "web";

    switch(type) {
    case "web":
      return Ci.nsIPrincipal.APP_STATUS_INSTALLED;
    case "privileged":
      return Ci.nsIPrincipal.APP_STATUS_PRIVILEGED;
    case "certified":
      return Ci.nsIPrincipal.APP_STATUS_CERTIFIED;
    default:
      throw new Error("Webapps.jsm: Undetermined app manifest type");
    }
  },

  /**
   * Determines if an update or a factory reset occured.
   */
  isFirstRun: function isFirstRun(aPrefBranch) {
    let savedmstone = null;
    try {
      savedmstone = aPrefBranch.getCharPref("gecko.mstone");
    } catch (e) {}

    let mstone = Services.appinfo.platformVersion;

    let savedBuildID = null;
    try {
      savedBuildID = aPrefBranch.getCharPref("gecko.buildID");
    } catch (e) {}

    let buildID = Services.appinfo.platformBuildID;

    aPrefBranch.setCharPref("gecko.mstone", mstone);
    aPrefBranch.setCharPref("gecko.buildID", buildID);

    return ((mstone != savedmstone) || (buildID != savedBuildID));
  },
}

/**
 * Helper object to access manifest information with locale support
 */
let ManifestHelper = function(aManifest, aOrigin) {
  this._origin = Services.io.newURI(aOrigin, null, null);
  this._manifest = aManifest;
  let chrome = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIXULChromeRegistry)
                                                          .QueryInterface(Ci.nsIToolkitChromeRegistry);
  let locale = chrome.getSelectedLocale("browser").toLowerCase();
  this._localeRoot = this._manifest;

  if (this._manifest.locales && this._manifest.locales[locale]) {
    this._localeRoot = this._manifest.locales[locale];
  }
  else if (this._manifest.locales) {
    // try with the language part of the locale ("en" for en-GB) only
    let lang = locale.split('-')[0];
    if (lang != locale && this._manifest.locales[lang])
      this._localeRoot = this._manifest.locales[lang];
  }
};

ManifestHelper.prototype = {
  _localeProp: function(aProp) {
    if (this._localeRoot[aProp] != undefined)
      return this._localeRoot[aProp];
    return this._manifest[aProp];
  },

  get name() {
    return this._localeProp("name");
  },

  get description() {
    return this._localeProp("description");
  },

  get version() {
    return this._localeProp("version");
  },

  get launch_path() {
    return this._localeProp("launch_path");
  },

  get developer() {
    return this._localeProp("developer");
  },

  get icons() {
    return this._localeProp("icons");
  },

  get appcache_path() {
    return this._localeProp("appcache_path");
  },

  get orientation() {
    return this._localeProp("orientation");
  },

  get package_path() {
    return this._localeProp("package_path");
  },

  get size() {
    return this._manifest["size"] || 0;
  },

  get permissions() {
    if (this._manifest.permissions) {
      return this._manifest.permissions;
    }
    return {};
  },

  iconURLForSize: function(aSize) {
    let icons = this._localeProp("icons");
    if (!icons)
      return null;
    let dist = 100000;
    let icon = null;
    for (let size in icons) {
      let iSize = parseInt(size);
      if (Math.abs(iSize - aSize) < dist) {
        icon = this._origin.resolve(icons[size]);
        dist = Math.abs(iSize - aSize);
      }
    }
    return icon;
  },

  fullLaunchPath: function(aStartPoint) {
    // If no start point is specified, we use the root launch path.
    // In all error cases, we just return null.
    if ((aStartPoint || "") === "") {
      return this._origin.resolve(this._localeProp("launch_path") || "");
    }

    // Search for the l10n entry_points property.
    let entryPoints = this._localeProp("entry_points");
    if (!entryPoints) {
      return null;
    }

    if (entryPoints[aStartPoint]) {
      return this._origin.resolve(entryPoints[aStartPoint].launch_path || "");
    }

    return null;
  },

  resolveFromOrigin: function(aURI) {
    return this._origin.resolve(aURI);
  },

  fullAppcachePath: function() {
    let appcachePath = this._localeProp("appcache_path");
    return this._origin.resolve(appcachePath ? appcachePath : "");
  },

  fullPackagePath: function() {
    let packagePath = this._localeProp("package_path");
    return this._origin.resolve(packagePath ? packagePath : "");
  }
}
