/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Don't modify this, instead set dom.push.debug.
var gDebuggingEnabled = false;

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

    return this.createPromise((resolve, reject) => {
      function permissionDenied() {
        reject("PermissionDeniedError");
      }

      let permission = Ci.nsIPermissionManager.UNKNOWN_ACTION;
      try {
        permission = this._testPermission();
      } catch (e) {
        permissionDenied();
        return;
      }

      if (permission == Ci.nsIPermissionManager.ALLOW_ACTION) {
        resolve();
      } else if (permission == Ci.nsIPermissionManager.DENY_ACTION) {
        permissionDenied();
      } else {
        this._requestPermission(resolve, permissionDenied);
      }
    });
  },

  subscribe: function() {
    debug("subscribe()");

    let histogram = Services.telemetry.getHistogramById("PUSH_API_USED");
    histogram.add(true);
    return this.askPermission().then(() =>
      this.createPromise((resolve, reject) => {
        let callback = new PushEndpointCallback(this, resolve, reject);
        this._client.subscribe(this._scope, this._principal, callback);
      })
    );
  },

  getSubscription: function() {
    debug("getSubscription()" + this._scope);

    return this.createPromise((resolve, reject) => {
      let callback = new PushEndpointCallback(this, resolve, reject);
      this._client.getSubscription(this._scope, this._principal, callback);
    });
  },

  permissionState: function() {
    debug("permissionState()" + this._scope);

    return this.createPromise((resolve, reject) => {
      let permission = Ci.nsIPermissionManager.UNKNOWN_ACTION;

      try {
        permission = this._testPermission();
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
  },

  _testPermission: function() {
    return Services.perms.testExactPermissionFromPrincipal(
      this._principal, "desktop-notification");
  },

  _requestPermission: function(allowCallback, cancelCallback) {
    // Create an array with a single nsIContentPermissionType element.
    let type = {
      type: "desktop-notification",
      access: null,
      options: [],
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPermissionType]),
    };
    let typeArray = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    typeArray.appendElement(type, false);

    // create a nsIContentPermissionRequest
    let request = {
      types: typeArray,
      principal: this._principal,
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPermissionRequest]),
      allow: function() {
        let histogram = Services.telemetry.getHistogramById("PUSH_API_PERMISSION_GRANTED");
        histogram.add();
        allowCallback();
      },
      cancel: function() {
        let histogram = Services.telemetry.getHistogramById("PUSH_API_PERMISSION_DENIED");
        histogram.add();
        cancelCallback();
      },
      window: this._window,
    };

    let histogram = Services.telemetry.getHistogramById("PUSH_API_PERMISSION_REQUESTED");
    histogram.add(1);
    // Using askPermission from nsIDOMWindowUtils that takes care of the
    // remoting if needed.
    let windowUtils = this._window.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDOMWindowUtils);
    windowUtils.askPermission(request);
  },
};

function PushEndpointCallback(pushManager, resolve, reject) {
  this.pushManager = pushManager;
  this.resolve = resolve;
  this.reject = reject;
}

PushEndpointCallback.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPushEndpointCallback]),
  onPushEndpoint: function(ok, endpoint, keyLen, key) {
    let {pushManager} = this;
    if (!Components.isSuccessCode(ok)) {
      this.reject("AbortError");
      return;
    }

    if (!endpoint) {
      this.resolve(null);
      return;
    }

    let publicKey = null;
    if (keyLen) {
      publicKey = new ArrayBuffer(keyLen);
      let keyView = new Uint8Array(publicKey);
      keyView.set(key);
    }

    let sub = new pushManager._window.PushSubscription(endpoint,
                                                       pushManager._scope,
                                                       publicKey);
    sub.setPrincipal(pushManager._principal);
    this.resolve(sub);
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([Push]);
