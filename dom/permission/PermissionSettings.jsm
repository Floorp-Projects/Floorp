/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let DEBUG = 0;
if (DEBUG)
  debug = function (s) { dump("-*- PermissionSettings Module: " + s + "\n"); }
else
  debug = function (s) {}

const Cu = Components.utils; 
const Cc = Components.classes;
const Ci = Components.interfaces;

let EXPORTED_SYMBOLS = ["PermissionSettingsModule"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageListenerManager");

var permissionManager = Cc["@mozilla.org/permissionmanager;1"].getService(Ci.nsIPermissionManager);
var secMan = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(Ci.nsIScriptSecurityManager);
var appsService = Cc["@mozilla.org/AppsService;1"].getService(Ci.nsIAppsService);

let PermissionSettingsModule = {
  init: function() {
    debug("Init");
    ppmm.addMessageListener("PermissionSettings:AddPermission", this);
    Services.obs.addObserver(this, "profile-before-change", false);
  },

  addPermission: function(aData, aCallbacks) {
    let uri = Services.io.newURI(aData.origin, null, null);
    let appID = appsService.getAppLocalIdByManifestURL(aData.manifestURL);
    let principal = secMan.getAppCodebasePrincipal(uri, appID, aData.browserFlag);

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
    debug("add: " + aData.origin + " " + appID + " " + action);
    permissionManager.addFromPrincipal(principal, aData.type, action);
  },

  observe: function(aSubject, aTopic, aData) {
    ppmm.removeMessageListener("PermissionSettings:AddPermission", this);
    Services.obs.removeObserver(this, "profile-before-change");
    ppmm = null;
  },

  receiveMessage: function(aMessage) {
    debug("PermissionSettings::receiveMessage " + aMessage.name);
    let mm = aMessage.target;
    let msg = aMessage.data;

    let result;
    switch (aMessage.name) {
      case "PermissionSettings:AddPermission":
        this.addPermission(msg);
        break;
    }
  }
}

PermissionSettingsModule.init();
