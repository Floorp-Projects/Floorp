/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/DOMRequestHelper.jsm");

XPCOMUtils.defineLazyGetter(this, "console", () => {
  let {ConsoleAPI} = ChromeUtils.import("resource://gre/modules/Console.jsm", {});
  return new ConsoleAPI({
    maxLogLevelPref: "dom.push.loglevel",
    prefix: "Push",
  });
});

XPCOMUtils.defineLazyServiceGetter(this, "PushService",
  "@mozilla.org/push/Service;1", "nsIPushService");

const PUSH_CID = Components.ID("{cde1d019-fad8-4044-b141-65fb4fb7a245}");

/**
 * The Push component runs in the child process and exposes the Push API
 * to the web application. The PushService running in the parent process is the
 * one actually performing all operations.
 */
function Push() {
  console.debug("Push()");
}

Push.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  contractID: "@mozilla.org/push/PushManager;1",

  classID : PUSH_CID,

  QueryInterface : ChromeUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer,
                                           Ci.nsISupportsWeakReference,
                                           Ci.nsIObserver]),

  init: function(win) {
    console.debug("init()");

    this._window = win;

    this.initDOMRequestHelper(win);

    this._principal = win.document.nodePrincipal;
  },

  __init: function(scope) {
    this._scope = scope;
  },

  askPermission: function () {
    console.debug("askPermission()");

    return this.createPromise((resolve, reject) => {
      let permissionDenied = () => {
        reject(new this._window.DOMException(
          "User denied permission to use the Push API.",
          "NotAllowedError"
        ));
      };

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

  subscribe: function(options) {
    console.debug("subscribe()", this._scope);

    return this.askPermission().then(() =>
      this.createPromise((resolve, reject) => {
        let callback = new PushSubscriptionCallback(this, resolve, reject);

        if (!options || options.applicationServerKey === null) {
          PushService.subscribe(this._scope, this._principal, callback);
          return;
        }

        let keyView = this._normalizeAppServerKey(options.applicationServerKey);
        if (keyView.byteLength === 0) {
          callback._rejectWithError(Cr.NS_ERROR_DOM_PUSH_INVALID_KEY_ERR);
          return;
        }
        PushService.subscribeWithKey(this._scope, this._principal,
                                     keyView.byteLength, keyView,
                                     callback);
      })
    );
  },

  _normalizeAppServerKey: function(appServerKey) {
    let key;
    if (typeof appServerKey == "string") {
      try {
        key = Cu.cloneInto(ChromeUtils.base64URLDecode(appServerKey, {
          padding: "reject",
        }), this._window);
      } catch (e) {
        throw new this._window.DOMException(
          "String contains an invalid character",
          "InvalidCharacterError"
        );
      }
    } else if (this._window.ArrayBuffer.isView(appServerKey)) {
      key = appServerKey.buffer;
    } else {
      // `appServerKey` is an array buffer.
      key = appServerKey;
    }
    return new this._window.Uint8Array(key);
  },

  getSubscription: function() {
    console.debug("getSubscription()", this._scope);

    return this.createPromise((resolve, reject) => {
      let callback = new PushSubscriptionCallback(this, resolve, reject);
      PushService.getSubscription(this._scope, this._principal, callback);
    });
  },

  permissionState: function() {
    console.debug("permissionState()", this._scope);

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
    let permission = Services.perms.testExactPermissionFromPrincipal(
      this._principal, "desktop-notification");
    if (permission == Ci.nsIPermissionManager.ALLOW_ACTION) {
      return permission;
    }
    try {
      if (Services.prefs.getBoolPref("dom.push.testing.ignorePermission")) {
        permission = Ci.nsIPermissionManager.ALLOW_ACTION;
      }
    } catch (e) {}
    return permission;
  },

  _requestPermission: function(allowCallback, cancelCallback) {
    // Create an array with a single nsIContentPermissionType element.
    let type = {
      type: "desktop-notification",
      access: null,
      options: [],
      QueryInterface: ChromeUtils.generateQI([Ci.nsIContentPermissionType]),
    };
    let typeArray = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    typeArray.appendElement(type);

    // create a nsIContentPermissionRequest
    let request = {
      types: typeArray,
      principal: this._principal,
      QueryInterface: ChromeUtils.generateQI([Ci.nsIContentPermissionRequest]),
      allow: allowCallback,
      cancel: cancelCallback,
      window: this._window,
    };

    // Using askPermission from nsIDOMWindowUtils that takes care of the
    // remoting if needed.
    let windowUtils = this._window.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDOMWindowUtils);
    windowUtils.askPermission(request);
  },
};

function PushSubscriptionCallback(pushManager, resolve, reject) {
  this.pushManager = pushManager;
  this.resolve = resolve;
  this.reject = reject;
}

PushSubscriptionCallback.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPushSubscriptionCallback]),

  onPushSubscription: function(ok, subscription) {
    let {pushManager} = this;
    if (!Components.isSuccessCode(ok)) {
      this._rejectWithError(ok);
      return;
    }

    if (!subscription) {
      this.resolve(null);
      return;
    }

    let p256dhKey = this._getKey(subscription, "p256dh");
    let authSecret = this._getKey(subscription, "auth");
    let options = {
      endpoint: subscription.endpoint,
      scope: pushManager._scope,
      p256dhKey: p256dhKey,
      authSecret: authSecret,
    };
    let appServerKey = this._getKey(subscription, "appServer");
    if (appServerKey) {
      // Avoid passing null keys to work around bug 1256449.
      options.appServerKey = appServerKey;
    }
    let sub = new pushManager._window.PushSubscription(options);
    this.resolve(sub);
  },

  _getKey: function(subscription, name) {
    let outKeyLen = {};
    let rawKey = Cu.cloneInto(subscription.getKey(name, outKeyLen),
                              this.pushManager._window);
    if (!outKeyLen.value) {
      return null;
    }

    let key = new this.pushManager._window.ArrayBuffer(outKeyLen.value);
    let keyView = new this.pushManager._window.Uint8Array(key);
    keyView.set(rawKey);
    return key;
  },

  _rejectWithError: function(result) {
    let error;
    switch (result) {
      case Cr.NS_ERROR_DOM_PUSH_INVALID_KEY_ERR:
        error = new this.pushManager._window.DOMException(
          "Invalid raw ECDSA P-256 public key.",
          "InvalidAccessError"
        );
        break;

      case Cr.NS_ERROR_DOM_PUSH_MISMATCHED_KEY_ERR:
        error = new this.pushManager._window.DOMException(
          "A subscription with a different application server key already exists.",
          "InvalidStateError"
        );
        break;

      default:
        error = new this.pushManager._window.DOMException(
          "Error retrieving push subscription.",
          "AbortError"
        );
    }
    this.reject(error);
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([Push]);
