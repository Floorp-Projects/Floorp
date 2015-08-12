/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Don't modify this, instead set dom.push.debug.
let gDebuggingEnabled = false;

function debug(s) {
  if (gDebuggingEnabled)
    dump("-*- Push.js: " + s + "\n");
}

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

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
                                          Ci.nsISupportsWeakReference,
                                          Ci.nsIObserver]),

  init: function(aWindow) {
    // Set debug first so that all debugging actually works.
    // NOTE: We don't add an observer here like in PushService. Flipping the
    // pref will require a reload of the app/page, which seems acceptable.
    gDebuggingEnabled = Services.prefs.getBoolPref("dom.push.debug");
    debug("init()");

    this._window = aWindow;

    this.initDOMRequestHelper(aWindow);

    this._principal = aWindow.document.nodePrincipal;

    this._client = Cc["@mozilla.org/push/PushClient;1"].createInstance(Ci.nsIPushClient);
  },

  setScope: function(scope){
    debug('setScope ' + scope);
    this._scope = scope;
  },

  askPermission: function (aAllowCallback, aCancelCallback) {
    debug("askPermission");

    let principal = this._window.document.nodePrincipal;
    let type = "push";
    let permValue =
      Services.perms.testExactPermissionFromPrincipal(principal, type);

    if (permValue == Ci.nsIPermissionManager.ALLOW_ACTION) {
        aAllowCallback();
      return;
    }

    if (permValue == Ci.nsIPermissionManager.DENY_ACTION) {
      aCancelCallback();
      return;
    }

    // Create an array with a single nsIContentPermissionType element.
    type = {
      type: "push",
      access: null,
      options: [],
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPermissionType])
    };
    let typeArray = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    typeArray.appendElement(type, false);

    // create a nsIContentPermissionRequest
    let request = {
      types: typeArray,
      principal: principal,
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPermissionRequest]),
      allow: function() {
        aAllowCallback();
      },
      cancel: function() {
        aCancelCallback();
      },
      window: this._window
    };

    // Using askPermission from nsIDOMWindowUtils that takes care of the
    // remoting if needed.
    let windowUtils = this._window.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDOMWindowUtils);
    windowUtils.askPermission(request);
  },

  getEndpointResponse: function(fn) {
    debug("GetEndpointResponse " + fn.toSource());
    let that = this;
    let p = this.createPromise(function(resolve, reject) {
      this.askPermission(
        () => {
          fn(that._scope, that._principal, {
            QueryInterface: XPCOMUtils.generateQI([Ci.nsIPushEndpointCallback]),
            onPushEndpoint: function(ok, endpoint) {
              if (ok === Cr.NS_OK) {
                if (endpoint) {
                  let sub = new that._window.PushSubscription(endpoint, that._scope);
                  sub.setPrincipal(that._principal);
                  resolve(sub);
                } else {
                  resolve(null);
                }
              } else {
                reject("AbortError");
              }
            }
          });
        },

        () => {
          reject("PermissionDeniedError");
        }
      );
    }.bind(this));
    return p;
  },

  subscribe: function() {
    debug("subscribe()");
    return this.getEndpointResponse(this._client.subscribe.bind(this._client));
  },

  getSubscription: function() {
    debug("getSubscription()" + this._scope);
    return this.getEndpointResponse(this._client.getSubscription.bind(this._client));
  },

  permissionState: function() {
    debug("permissionState()" + this._scope);

    let p = this.createPromise((resolve, reject) => {
      let permission = Ci.nsIPermissionManager.DENY_ACTION;

      try {
        let permissionManager = Cc["@mozilla.org/permissionmanager;1"]
                                .getService(Ci.nsIPermissionManager);
        permission =
          permissionManager.testExactPermissionFromPrincipal(this._principal,
                                                             "push");
      } catch(e) {
        reject();
        return;
      }

      let pushPermissionStatus = "prompt";
      if (permission == Ci.nsIPermissionManager.ALLOW_ACTION) {
        pushPermissionStatus = "granted";
      } else if (permission == Ci.nsIPermissionManager.DENY_ACTION) {
        pushPermissionStatus = "denied";
      }
      resolve(pushPermissionStatus);
    });
    return p;
  },
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([Push]);
