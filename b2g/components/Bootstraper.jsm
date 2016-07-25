/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Bootstraper"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const CC = Components.Constructor;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");

function debug(aMsg) {
  //dump("-*- Bootstraper: " + aMsg + "\n");
}

/**
  * This module loads the manifest for app from the --start-url enpoint and
  * ensures that it's installed as the system app.
  */
this.Bootstraper = {
  _manifestURL: null,
  _startupURL: null,

  bailout: function(aMsg) {
    dump("************************************************************\n");
    dump("* /!\\ " + aMsg + "\n");
    dump("************************************************************\n");
    let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"]
                       .getService(Ci.nsIAppStartup);
    appStartup.quit(appStartup.eForceQuit);
  },

  installSystemApp: function(aManifest) {
    // Get the appropriate startup url from the manifest launch_path.
    let base = Services.io.newURI(this._manifestURL, null, null);
    let origin = base.prePath;
    let helper = new ManifestHelper(aManifest, origin, this._manifestURL);
    this._startupURL = helper.fullLaunchPath();

    return new Promise((aResolve, aReject) => {
      debug("Origin is " + origin);
      let appData = {
        app: {
          installOrigin: origin,
          origin: origin,
          manifest: aManifest,
          manifestURL: this._manifestURL,
          manifestHash: AppsUtils.computeHash(JSON.stringify(aManifest)),
          appStatus: Ci.nsIPrincipal.APP_STATUS_CERTIFIED
        },
        appId: 1,
        isBrowser: false,
        isPackage: false
      };

      //DOMApplicationRegistry.confirmInstall(appData, null, aResolve);
    });
  },

  /**
    * Resolves to a json manifest.
    */
  loadManifest: function() {
    return new Promise((aResolve, aReject) => {
      debug("Loading manifest " + this._manifestURL);

      let xhr =  Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                 .createInstance(Ci.nsIXMLHttpRequest);
      xhr.mozBackgroundRequest = true;
      xhr.open("GET", this._manifestURL);
      xhr.responseType = "json";
      xhr.addEventListener("load", () => {
        if (xhr.status >= 200 && xhr.status < 400) {
          debug("Success loading " + this._manifestURL);
          aResolve(xhr.response);
        } else {
          aReject("Error loading " + this._manifestURL);
        }
      });
      xhr.addEventListener("error", () => {
        aReject("Error loading " + this._manifestURL);
      });
      xhr.send(null);
    });
  },

  configure: function() {
    debug("Setting startup prefs... " + this._startupURL);
    Services.prefs.setCharPref("b2g.system_manifest_url", this._manifestURL);
    Services.prefs.setCharPref("b2g.system_startup_url", this._startupURL);
    return Promise.resolve();
  },

  /**
    * If a system app is already installed, uninstall it so that we can
    * cleanly replace it by the current one.
    */
  uninstallPreviousSystemApp: function() {
    // TODO: FIXME
    return Promise.resolve();

    let oldManifestURL;
    try{
      oldManifestURL = Services.prefs.getCharPref("b2g.system_manifest_url");
    } catch(e) {
      // No preference set, so nothing to uninstall.
      return Promise.resolve();
    }

    let id = DOMApplicationRegistry.getAppLocalIdByManifestURL(oldManifestURL);
    if (id == Ci.nsIScriptSecurityManager.NO_APP_ID) {
      return Promise.resolve();
    }
    debug("Uninstalling " + oldManifestURL);
    return DOMApplicationRegistry.uninstall(oldManifestURL);
  },

  /**
   * Check if we are already configured to run from this manifest url.
   */
  isInstallRequired: function(aManifestURL) {
    try {
      if (Services.prefs.getCharPref("b2g.system_manifest_url") == aManifestURL) {
        return false;
      }
    } catch(e) { }
    return true;
  },

  /**
    * Resolves once we have installed the app.
    */
  ensureSystemAppInstall: function(aManifestURL) {
    this._manifestURL = aManifestURL;
    debug("Installing app from " + this._manifestURL);

    if (!this.isInstallRequired(this._manifestURL)) {
      debug("Already configured for " + this._manifestURL);
      return Promise.resolve();
    }

    return new Promise((aResolve, aReject) => {
      this.uninstallPreviousSystemApp.bind(this)
          .then(this.loadManifest.bind(this))
          .then(this.installSystemApp.bind(this))
          .then(this.configure.bind(this))
          .then(aResolve)
          .catch(aReject);
    });
  }
};
