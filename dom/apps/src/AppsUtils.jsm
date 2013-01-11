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

XPCOMUtils.defineLazyGetter(this, "NetUtil", function() {
  return Cc["@mozilla.org/network/util;1"]
           .getService(Ci.nsINetUtil);
});

// Shared code for AppsServiceChild.jsm, Webapps.jsm and Webapps.js

this.EXPORTED_SYMBOLS = ["AppsUtils", "ManifestHelper"];

function debug(s) {
  //dump("-*- AppsUtils.jsm: " + s + "\n");
}

function isAbsoluteURI(aURI) {
  let foo = Services.io.newURI("http://foo", null, null);
  let bar = Services.io.newURI("http://bar", null, null);
  return Services.io.newURI(aURI, null, foo).prePath != foo.prePath ||
         Services.io.newURI(aURI, null, bar).prePath != bar.prePath;
}

this.AppsUtils = {
  // Clones a app, without the manifest.
  cloneAppObject: function cloneAppObject(aApp) {
    return {
      name: aApp.name,
      csp: aApp.csp,
      installOrigin: aApp.installOrigin,
      origin: aApp.origin,
      receipts: aApp.receipts ? JSON.parse(JSON.stringify(aApp.receipts)) : null,
      installTime: aApp.installTime,
      manifestURL: aApp.manifestURL,
      appStatus: aApp.appStatus,
      removable: aApp.removable,
      id: aApp.id,
      localId: aApp.localId,
      basePath: aApp.basePath,
      progress: aApp.progress || 0.0,
      installState: aApp.installState || "installed",
      downloadAvailable: aApp.downloadAvailable,
      downloading: aApp.downloading,
      readyToApplyDownload: aApp.readyToApplyDownload,
      downloadSize: aApp.downloadSize || 0,
      lastUpdateCheck: aApp.lastUpdateCheck,
      updateTime: aApp.updateTime,
      etag: aApp.etag,
      packageEtag: aApp.packageEtag,
      installerAppId: aApp.installerAppId || Ci.nsIScriptSecurityManager.NO_APP_ID,
      installerIsBrowser: !!aApp.installerIsBrowser
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

  getCSPByLocalId: function getCSPByLocalId(aApps, aLocalId) {
    debug("getCSPByLocalId " + aLocalId);
    for (let id in aApps) {
      let app = aApps[id];
      if (app.localId == aLocalId) {
	  return ( app.csp || "" );
      }
    }

    return "";
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
  checkManifest: function(aManifest, app) {
    if (aManifest.name == undefined)
      return false;

    // launch_path, entry_points launch paths, message hrefs, and activity hrefs can't be absolute
    if (aManifest.launch_path && isAbsoluteURI(aManifest.launch_path))
      return false;

    function checkAbsoluteEntryPoints(entryPoints) {
      for (let name in entryPoints) {
        if (entryPoints[name].launch_path && isAbsoluteURI(entryPoints[name].launch_path)) {
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

    if (aManifest.activities) {
      for (let activityName in aManifest.activities) {
        let activity = aManifest.activities[activityName];
        if (activity.href && isAbsoluteURI(activity.href)) {
          return false;
        }
      }
    }

    // |messages| is an array of items, where each item is either a string or
    // a {name: href} object.
    let messages = aManifest.messages;
    if (messages) {
      if (!Array.isArray(messages)) {
        return false;
      }
      for (let item of aManifest.messages) {
        if (typeof item == "object") {
          let keys = Object.keys(item);
          if (keys.length != 1) {
            return false;
          }
          if (isAbsoluteURI(item[keys[0]])) {
            return false;
          }
        }
      }
    }

    // Ensure that non-updatable fields contains the current app value
    AppsUtils.normalizeManifest(aManifest, app);

    return true;
  },

  checkManifestContentType: function
     checkManifestContentType(aInstallOrigin, aWebappOrigin, aContentType) {
    let hadCharset = { };
    let charset = { };
    let contentType = NetUtil.parseContentType(aContentType, charset, hadCharset);
    if (aInstallOrigin != aWebappOrigin &&
        contentType != "application/x-web-app-manifest+json") {
      return false;
    }
    return true;
  },

  /**
   * Method to apply modifications to webapp manifests file saved internally.
   * For now, only ensure app can't rename itself.
   */
  normalizeManifest: function normalizeManifest(aManifest, aApp) {
    // As normalizeManifest isn't only called on update but also
    // during app install, we need to bail out on install.
    if (aApp.installState != "installed" &&
        aApp.installState != "updating") {
      return;
    }

    let previousManifest = aApp.manifest;

    // Ensure that app name can't be updated
    aManifest.name = aApp.name;

    // Nor through localized names
    if ('locales' in aManifest) {
      let defaultName = new ManifestHelper(aManifest, aApp.origin).name;
      for (let locale in aManifest.locales) {
        let entry = aManifest.locales[locale];
        if (!entry.name) {
          continue;
        }
        // In case previous manifest didn't had a name,
        // we use the default app name
        let localizedName = defaultName;
        if (previousManifest && 'locales' in previousManifest &&
            locale in previousManifest.locales) {
          localizedName = previousManifest.locales[locale].name;
        }
        entry.name = localizedName;
      }
    }
  },

  /**
   * Determines whether the manifest allows installs for the given origin.
   * @param object aManifest
   * @param string aInstallOrigin
   * @return boolean
   **/
  checkInstallAllowed: function checkInstallAllowed(aManifest, aInstallOrigin) {
    if (!aManifest.installs_allowed_from) {
      return true;
    }

    function cbCheckAllowedOrigin(aOrigin) {
      return aOrigin == "*" || aOrigin == aInstallOrigin;
    }

    return aManifest.installs_allowed_from.some(cbCheckAllowedOrigin);
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

  /**
   * Check if two manifests have the same set of properties and that the
   * values of these properties are the same, in each locale.
   * Manifests here are raw json ones.
   */
  compareManifests: function compareManifests(aManifest1, aManifest2) {
    // 1. check if we have the same locales in both manifests.
    let locales1 = [];
    let locales2 = [];
    if (aManifest1.locales) {
      for (let locale in aManifest1.locales) {
        locales1.push(locale);
      }
    }
    if (aManifest2.locales) {
      for (let locale in aManifest2.locales) {
        locales2.push(locale);
      }
    }
    if (locales1.sort().join() !== locales2.sort().join()) {
      return false;
    }

    // Helper function to check the app name and developer information for
    // two given roots.
    let checkNameAndDev = function(aRoot1, aRoot2) {
      let name1 = aRoot1.name;
      let name2 = aRoot2.name;
      if (name1 !== name2) {
        return false;
      }

      let dev1 = aRoot1.developer;
      let dev2 = aRoot2.developer;
      if ((dev1 && !dev2) || (dev2 && !dev1)) {
        return false;
      }

      return (!dev1 && !dev2) ||
             (dev1.name === dev2.name && dev1.url === dev2.url);
    }

    // 2. For each locale, check if the name and dev info are the same.
    if (!checkNameAndDev(aManifest1, aManifest2)) {
      return false;
    }

    for (let locale in aManifest1.locales) {
      if (!checkNameAndDev(aManifest1.locales[locale],
                           aManifest2.locales[locale])) {
        return false;
      }
    }

    // Nothing failed.
    return true;
  }
}

/**
 * Helper object to access manifest information with locale support
 */
this.ManifestHelper = function(aManifest, aOrigin) {
  this._origin = Services.io.newURI(aOrigin, null, null);
  this._manifest = aManifest;
  let chrome = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIXULChromeRegistry)
                                                          .QueryInterface(Ci.nsIToolkitChromeRegistry);
  let locale = chrome.getSelectedLocale("global").toLowerCase();
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
    // Default to {} in order to avoid exception in code
    // that doesn't check for null `developer`
    return this._localeProp("developer") || {};
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
    // This should be enforced higher up, but check it here just in case.
    if (isAbsoluteURI(aURI)) {
      throw new Error("Webapps.jsm: non-relative URI passed to resolveFromOrigin");
    }
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
