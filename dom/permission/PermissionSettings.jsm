/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function debug(s) {
  //dump("-*- PermissionSettings Module: " + s + "\n");
}

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

this.EXPORTED_SYMBOLS = ["PermissionSettingsModule"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PermissionsTable.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageListenerManager");

XPCOMUtils.defineLazyServiceGetter(this,
                                   "appsService",
                                   "@mozilla.org/AppsService;1",
                                   "nsIAppsService");

this.PermissionSettingsModule = {
  init: function init() {
    debug("Init");
    ppmm.addMessageListener("PermissionSettings:AddPermission", this);
    Services.obs.addObserver(this, "profile-before-change", false);
  },


  _isChangeAllowed: function(aPrincipal, aPermName, aAction) {
    // Bug 812289:
    // Change is allowed from a child process when all of the following
    // conditions stand true:
    //   * the action isn't "unknown" (so the change isn't a delete) if the app
    //     is installed
    //   * the permission already exists on the database
    //   * the permission is marked as explicit on the permissions table
    // Note that we *have* to check the first two conditions here because
    // permissionManager doesn't know if it's being called as a result of
    // a parent process or child process request. We could check
    // if the permission is actually explicit (and thus modifiable) or not
    // on permissionManager also but we currently don't.
    let perm =
      Services.perms.testExactPermissionFromPrincipal(aPrincipal,aPermName);
    let isExplicit = isExplicitInPermissionsTable(aPermName, aPrincipal.appStatus);

    return (aAction === "unknown" &&
            aPrincipal.appStatus === Ci.nsIPrincipal.APP_STATUS_NOT_INSTALLED) ||
           (aAction !== "unknown" &&
            (perm !== Ci.nsIPermissionManager.UNKNOWN_ACTION) &&
            isExplicit);
  },

  addPermission: function addPermission(aData, aCallbacks) {

    this._internalAddPermission(aData, true, aCallbacks);

  },


  _internalAddPermission: function _internalAddPermission(aData, aAllowAllChanges, aCallbacks) {
    // TODO: Bug 1196644 - Add signPKg parameter into PermissionSettings.jsm.
    let app;
    let principal;
    // Test if app is cached (signed streamable package) or installed via DOMApplicationRegistry
    if (aData.isCachedPackage) {
      // If the app is from packaged web app, the origin includes origin attributes already.
      principal =
        Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(aData.origin);
      app = {localId: principal.appId};
    } else {
      app = appsService.getAppByManifestURL(aData.manifestURL);
      let uri = Services.io.newURI(aData.origin, null, null);
      principal =
        Services.scriptSecurityManager.createCodebasePrincipal(uri,
                                                               {appId: app.localId,
                                                                inIsolatedMozBrowser: aData.browserFlag});
    }

    let action;
    switch (aData.value)
    {
      case "unknown":
        action = Ci.nsIPermissionManager.UNKNOWN_ACTION;
        break;
      case "allow":
        action = Ci.nsIPermissionManager.ALLOW_ACTION;
        break;
      case "deny":
        action = Ci.nsIPermissionManager.DENY_ACTION;
        break;
      case "prompt":
        action = Ci.nsIPermissionManager.PROMPT_ACTION;
        break;
      default:
        dump("Unsupported PermisionSettings Action: " + aData.value +"\n");
        action = Ci.nsIPermissionManager.UNKNOWN_ACTION;
    }

    if (aAllowAllChanges ||
        this._isChangeAllowed(principal, aData.type, aData.value)) {
      debug("add: " + aData.origin + " " + app.localId + " " + action);
      Services.perms.addFromPrincipal(principal, aData.type, action);
      return true;
    } else {
      debug("add Failure: " + aData.origin + " " + app.localId + " " + action);
      return false; // This isn't currently used, see comment on setPermission
    }
  },

  getPermission: function getPermission(aPermName, aManifestURL, aOrigin, aBrowserFlag, aIsCachedPackage) {
    // TODO: Bug 1196644 - Add signPKg parameter into PermissionSettings.jsm
    debug("getPermission: " + aPermName + ", " + aManifestURL + ", " + aOrigin);
    let principal;
    // Test if app is cached (signed streamable package) or installed via DOMApplicationRegistry
    if (aIsCachedPackage) {
      // If the app is from packaged web app, the origin includes origin attributes already.
      principal =
        Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(aOrigin);
    } else {
      let uri = Services.io.newURI(aOrigin, null, null);
      let appID = appsService.getAppLocalIdByManifestURL(aManifestURL);
      principal =
        Services.scriptSecurityManager.createCodebasePrincipal(uri,
                                                               {appId: appID,
                                                                inIsolatedMozBrowser: aBrowserFlag});
    }
    let result = Services.perms.testExactPermissionFromPrincipal(principal, aPermName);
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

  removePermission: function removePermission(aPermName, aManifestURL, aOrigin, aBrowserFlag, aIsCachedPackage) {
    let data = {
      type: aPermName,
      origin: aOrigin,
      manifestURL: aManifestURL,
      value: "unknown",
      browserFlag: aBrowserFlag,
      isCachedPackage: aIsCachedPackage
    };
    this._internalAddPermission(data, true);
  },

  observe: function observe(aSubject, aTopic, aData) {
    ppmm.removeMessageListener("PermissionSettings:AddPermission", this);
    Services.obs.removeObserver(this, "profile-before-change");
    ppmm = null;
  },

  receiveMessage: function receiveMessage(aMessage) {
    debug("PermissionSettings::receiveMessage " + aMessage.name);
    let mm = aMessage.target;
    let msg = aMessage.data;

    let result;
    switch (aMessage.name) {
      case "PermissionSettings:AddPermission":
        this._internalAddPermission(msg, false);
        break;
    }
  }
}

PermissionSettingsModule.init();
