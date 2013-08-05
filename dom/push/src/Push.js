/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function debug(s) {
  // dump("-*- Push.js: " + s + "\n");
}

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");

const PUSH_CID = Components.ID("{cde1d019-fad8-4044-b141-65fb4fb7a245}");

/**
 * The Push component runs in the child process and exposes the SimplePush API
 * to the web application. The PushService running in the parent process is the
 * one actually performing all operations.
 */
function Push() {
  debug("Push Constructor");
}

Push.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  contractID: "@mozilla.org/push/PushManager;1",

  classID : PUSH_CID,

  QueryInterface : XPCOMUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer,
                                          Ci.nsISupportsWeakReference]),

  init: function(aWindow) {
    debug("init()");

    let principal = aWindow.document.nodePrincipal;

    this._pageURL = principal.URI;

    let appsService = Cc["@mozilla.org/AppsService;1"]
                        .getService(Ci.nsIAppsService);
    this._app = appsService.getAppByLocalId(principal.appId);
    this._manifestURL = appsService.getManifestURLByLocalId(principal.appId);
    if (!this._manifestURL) {
	// Now what?  XXXbz should this be tested in a Func for this
	// interface so it wouldn't appear at all?
	return;
    }

    let perm = Services.perms.testExactPermissionFromPrincipal(principal,
                                                               "push");
    if (perm != Ci.nsIPermissionManager.ALLOW_ACTION) {
	// Now what?  XXXbz should this be tested in a Func for this
	// interface so it wouldn't appear at all?
	return;
    }

    this.initDOMRequestHelper(aWindow, [
      "PushService:Register:OK",
      "PushService:Register:KO",
      "PushService:Unregister:OK",
      "PushService:Unregister:KO",
      "PushService:Registrations:OK",
      "PushService:Registrations:KO"
    ]);

    this._cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"]
                   .getService(Ci.nsISyncMessageSender);
  },

  receiveMessage: function(aMessage) {
    debug("receiveMessage()");
    let request = this.getRequest(aMessage.data.requestID);
    let json = aMessage.data;
    if (!request) {
      debug("No request " + json.requestID);
      return;
    }

    switch (aMessage.name) {
      case "PushService:Register:OK":
        Services.DOMRequest.fireSuccess(request, json.pushEndpoint);
        break;
      case "PushService:Register:KO":
        Services.DOMRequest.fireError(request, json.error);
        break;
      case "PushService:Unregister:OK":
        Services.DOMRequest.fireSuccess(request, json.pushEndpoint);
        break;
      case "PushService:Unregister:KO":
        Services.DOMRequest.fireError(request, json.error);
        break;
      case "PushService:Registrations:OK":
        Services.DOMRequest.fireSuccess(request, json.registrations);
        break;
      case "PushService:Registrations:KO":
        Services.DOMRequest.fireError(request, json.error);
        break;
      default:
        debug("NOT IMPLEMENTED! receiveMessage for " + aMessage.name);
    }
  },

  register: function() {
    debug("register()");
    var req = this.createRequest();
    if (!Services.prefs.getBoolPref("services.push.connection.enabled")) {
      // If push socket is disabled by the user, immediately error rather than
      // timing out.
      Services.DOMRequest.fireErrorAsync(req, "NetworkError");
      return req;
    }

    this._cpmm.sendAsyncMessage("Push:Register", {
                                  pageURL: this._pageURL.spec,
                                  manifestURL: this._manifestURL,
                                  requestID: this.getRequestId(req)
                                });
    return req;
  },

  unregister: function(aPushEndpoint) {
    debug("unregister(" + aPushEndpoint + ")");
    var req = this.createRequest();
    this._cpmm.sendAsyncMessage("Push:Unregister", {
                                  pageURL: this._pageURL.spec,
                                  manifestURL: this._manifestURL,
                                  requestID: this.getRequestId(req),
                                  pushEndpoint: aPushEndpoint
                                });
    return req;
  },

  registrations: function() {
    debug("registrations()");
    var req = this.createRequest();
    this._cpmm.sendAsyncMessage("Push:Registrations", {
                                  manifestURL: this._manifestURL,
                                  requestID: this.getRequestId(req)
                                });
    return req;
  }
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([Push]);
