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

// Shared code for AppsServiceChild.jsm and Webapps.jsm

let EXPORTED_SYMBOLS = ["AppsUtils"];

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
      status: aApp.status || "installed"
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
  }
}
