/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* PermissionPromptHelper checks the permissionDB for a given permission
 * name and performs prompting if needed.
 * Usage: send PermissionPromptHelper:AskPermission via the FrameMessageManager with:
 * |origin|, |appID|, |browserFlag| -> used for getting the principal and
 * |type| and |access| to call testExactPermissionFromPrincipal.
 * Note that |access| isn't currently used.
 * Other arugments are:
 * requestID: ID that gets returned with the result message.
 *
 * Once the permission is checked, it returns with the message
 * "PermissionPromptHelper:AskPermission:OK"
 * The result contains the |result| e.g.Ci.nsIPermissionManager.ALLOW_ACTION
 * and a requestID that
 */

"use strict";

let DEBUG = 0;
if (DEBUG)
  debug = function (s) { dump("-*- Permission Prompt Helper component: " + s + "\n"); }
else
  debug = function (s) {}

const Cu = Components.utils; 
const Cc = Components.classes;
const Ci = Components.interfaces;

let EXPORTED_SYMBOLS = ["PermissionPromptHelper"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "ppmm", function() {
  return Cc["@mozilla.org/parentprocessmessagemanager;1"].getService(Ci.nsIFrameMessageManager);
});

var permissionManager = Cc["@mozilla.org/permissionmanager;1"].getService(Ci.nsIPermissionManager);
var secMan = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(Ci.nsIScriptSecurityManager);
var appsService = Cc["@mozilla.org/AppsService;1"].getService(Ci.nsIAppsService);

let PermissionPromptHelper = {
  init: function() {
    debug("Init");
    ppmm.addMessageListener("PermissionPromptHelper:AskPermission", this);
    Services.obs.addObserver(this, "profile-before-change", false);
  },

  askPermission: function(aMessage, aCallbacks) {
    let msg = aMessage.json;
    debug("askPerm: " + JSON.stringify(aMessage.json));
    let uri = Services.io.newURI(msg.origin, null, null);
    let principal = secMan.getAppCodebasePrincipal(uri, msg.appID, msg.browserFlag);
    let perm = permissionManager.testExactPermissionFromPrincipal(principal, msg.type);

    switch(perm) {
      case Ci.nsIPermissionManager.ALLOW_ACTION:
        aCallbacks.allow();
        return;
      case Ci.nsIPermissionManager.DENY_ACTION:
        aCallbacks.cancel();
        return;
    }

  // FIXXMEE PROMPT MAGIC! Bug 773114.
  // We have to diplay the prompt here.
  },

  observe: function(aSubject, aTopic, aData) {
    ppmm.removeMessageListener("PermissionPromptHelper:AskPermission", this);
    Services.obs.removeObserver(this, "profile-before-change");
    ppmm = null;
  },

  receiveMessage: function(aMessage) {
    debug("PermissionPromptHelper::receiveMessage " + aMessage.name);
    let mm = aMessage.target.QueryInterface(Ci.nsIFrameMessageManager);
    let msg = aMessage.data;

    let result;
    if (aMessage.name == "PermissionPromptHelper:AskPermission") {
      this.askPermission(aMessage, {
        cancel: function() {
          mm.sendAsyncMessage("PermissionPromptHelper:AskPermission:OK", {result: Ci.nsIPermissionManager.DENY_ACTION, requestID: msg.requestID});
        },
        allow: function() {
          mm.sendAsyncMessage("PermissionPromptHelper:AskPermission:OK", {result: Ci.nsIPermissionManager.ALLOW_ACTION, requestID: msg.requestID});
        }
      });
    }
  }
}

PermissionPromptHelper.init();
