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
let debug;
if (DEBUG)
  debug = function (s) { dump("-*- Permission Prompt Helper component: " + s + "\n"); }
else
  debug = function (s) {}

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

this.EXPORTED_SYMBOLS = ["PermissionPromptHelper"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageListenerManager");

XPCOMUtils.defineLazyServiceGetter(this, "permissionPromptService",
                                   "@mozilla.org/permission-prompt-service;1",
                                   "nsIPermissionPromptService");

let appsService = Cc["@mozilla.org/AppsService;1"].getService(Ci.nsIAppsService);

this.PermissionPromptHelper = {
  init: function init() {
    debug("Init");
    ppmm.addMessageListener("PermissionPromptHelper:AskPermission", this);
    Services.obs.addObserver(this, "profile-before-change", false);
  },

  askPermission: function askPermission(aMessage, aCallbacks) {
    let msg = aMessage.json;

    let access = msg.type;
    if (msg.access) {
      access = access + "-" + msg.access;
    }

    let uri = Services.io.newURI(msg.origin, null, null);
    let principal =
      Services.scriptSecurityManager.getAppCodebasePrincipal(uri, msg.appID, msg.browserFlag);

    let permValue =
      Services.perms.testExactPermissionFromPrincipal(principal, access);

    if (permValue == Ci.nsIPermissionManager.DENY_ACTION ||
        permValue == Ci.nsIPermissionManager.UNKNOWN_ACTION) {
      aCallbacks.cancel();
      return;
    }

    if (permValue == Ci.nsIPermissionManager.PROMPT_ACTION) {
      // create a nsIContentPermissionRequest
      let request = {
        type: msg.type,
        access: msg.access ? msg.access : "unused",
        principal: principal,
        QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPermissionRequest]),
        allow: aCallbacks.allow,
        cancel: aCallbacks.cancel,
        window: Services.wm.getOuterWindowWithId(msg.windowID)
      };

      permissionPromptService.getPermission(request);
      return;
    }

    if (permValue == Ci.nsIPermissionManager.ALLOW_ACTION) {
      aCallbacks.allow();
      return;
    }
  },

  observe: function observe(aSubject, aTopic, aData) {
    ppmm.removeMessageListener("PermissionPromptHelper:AskPermission", this);
    Services.obs.removeObserver(this, "profile-before-change");
    ppmm = null;
  },

  receiveMessage: function receiveMessage(aMessage) {
    debug("PermissionPromptHelper::receiveMessage " + aMessage.name);
    let mm = aMessage.target;
    let msg = aMessage.data;

    let result;
    if (aMessage.name == "PermissionPromptHelper:AskPermission") {
      this.askPermission(aMessage, {
        cancel: function() {
          mm.sendAsyncMessage("PermissionPromptHelper:AskPermission:OK",
                              { result: Ci.nsIPermissionManager.DENY_ACTION,
                                requestID: msg.requestID });
        },
        allow: function() {
          mm.sendAsyncMessage("PermissionPromptHelper:AskPermission:OK",
                              { result: Ci.nsIPermissionManager.ALLOW_ACTION,
                                requestID: msg.requestID });
        }
      });
    }
  }
}

PermissionPromptHelper.init();
