/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function debug(aMsg) {
  //dump("-*- PermissionSettings.js: " + aMsg + "\n");
}

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PermissionsTable.jsm");

var cpm = Cc["@mozilla.org/childprocessmessagemanager;1"].getService(Ci.nsISyncMessageSender);

// PermissionSettings

const PERMISSIONSETTINGS_CONTRACTID = "@mozilla.org/permissionSettings;1";
const PERMISSIONSETTINGS_CID        = Components.ID("{cd2cf7a1-f4c1-487b-8c1b-1a71c7097431}");

function PermissionSettings()
{
  debug("Constructor");
}

XPCOMUtils.defineLazyServiceGetter(this,
                                   "appsService",
                                   "@mozilla.org/AppsService;1",
                                   "nsIAppsService");

PermissionSettings.prototype = {
  get: function get(aPermName, aManifestURL, aOrigin, aBrowserFlag) {
    // TODO: Bug 1196644 - Add signPKg parameter into PermissionSettings.js
    debug("Get called with: " + aPermName + ", " + aManifestURL + ", " + aOrigin + ", " + aBrowserFlag);
    let uri = Services.io.newURI(aOrigin, null, null);
    let appID = appsService.getAppLocalIdByManifestURL(aManifestURL);
    let principal =
      Services.scriptSecurityManager.createCodebasePrincipal(uri,
                                                             {appId: appID,
                                                              inBrowser: aBrowserFlag});
    let result = Services.perms.testExactPermanentPermission(principal, aPermName);

    switch (result)
    {
      case Ci.nsIPermissionManager.UNKNOWN_ACTION:
        return "unknown";
      case Ci.nsIPermissionManager.ALLOW_ACTION:
        return "allow";
      case Ci.nsIPermissionManager.DENY_ACTION:
        return "deny";
      case Ci.nsIPermissionManager.PROMPT_ACTION:
        return "prompt";
      default:
        dump("Unsupported PermissionSettings Action!\n");
        return "unknown";
    }
  },

  isExplicit: function isExplicit(aPermName, aManifestURL, aOrigin,
                                  aBrowserFlag) {
    // TODO: Bug 1196644 - Add signPKg parameter into PermissionSettings.js
    debug("isExplicit: " + aPermName + ", " + aManifestURL + ", " + aOrigin);
    let uri = Services.io.newURI(aOrigin, null, null);
    let app = appsService.getAppByManifestURL(aManifestURL);
    let principal = Services.scriptSecurityManager
      .createCodebasePrincipal(uri, {appId: app.localId, inBrowser: aBrowserFlag});

    return isExplicitInPermissionsTable(aPermName,
                                        principal.appStatus,
                                        app.kind);
  },

  set: function set(aPermName, aPermValue, aManifestURL, aOrigin,
                    aBrowserFlag) {
    debug("Set called with: " + aPermName + ", " + aManifestURL + ", " +
          aOrigin + ",  " + aPermValue + ", " + aBrowserFlag);
    let currentPermValue = this.get(aPermName, aManifestURL, aOrigin,
                                    aBrowserFlag);
    let action;
    // Check for invalid calls so that we throw an exception rather than get
    // killed by parent process
    if (currentPermValue === "unknown" ||
        aPermValue === "unknown" ||
        !this.isExplicit(aPermName, aManifestURL, aOrigin, aBrowserFlag)) {
      let errorMsg = "PermissionSettings.js: '" + aPermName + "'" +
                     " is an implicit permission for '" + aManifestURL +
                     "' or the permission isn't set";
      Cu.reportError(errorMsg);
      throw new Components.Exception(errorMsg);
    }

    cpm.sendSyncMessage("PermissionSettings:AddPermission", {
      type: aPermName,
      origin: aOrigin,
      manifestURL: aManifestURL,
      value: aPermValue,
      browserFlag: aBrowserFlag
    });
  },

  remove: function remove(aPermName, aManifestURL, aOrigin) {
    // TODO: Bug 1196644 - Add signPKg parameter into PermissionSettings.js
    let uri = Services.io.newURI(aOrigin, null, null);
    let appID = appsService.getAppLocalIdByManifestURL(aManifestURL);
    let principal =
      Services.scriptSecurityManager.createCodebasePrincipal(uri,
                                                             {appId: appID,
                                                              inBrowser: true});

    if (principal.appStatus !== Ci.nsIPrincipal.APP_STATUS_NOT_INSTALLED) {
      let errorMsg = "PermissionSettings.js: '" + aOrigin + "'" +
                     " is installed or permission is implicit, cannot remove '" +
                     aPermName + "'.";
      Cu.reportError(errorMsg);
      throw new Components.Exception(errorMsg);
    }

    // PermissionSettings.jsm handles delete when value is "unknown"
    cpm.sendSyncMessage("PermissionSettings:AddPermission", {
      type: aPermName,
      origin: aOrigin,
      manifestURL: aManifestURL,
      value: "unknown",
      browserFlag: true
    });
  },

  classID : PERMISSIONSETTINGS_CID,
  QueryInterface : XPCOMUtils.generateQI([])
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PermissionSettings])
