/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { DOMRequestIpcHelper } = ChromeUtils.import(
  "resource://gre/modules/DOMRequestHelper.jsm"
);

XPCOMUtils.defineLazyGetter(this, "console", () => {
  let { ConsoleAPI } = ChromeUtils.import("resource://gre/modules/Console.jsm");
  return new ConsoleAPI({
    maxLogLevelPref: "dom.push.loglevel",
    prefix: "Push",
  });
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "PushService",
  "@mozilla.org/push/Service;1",
  "nsIPushService"
);

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

  classID: PUSH_CID,

  QueryInterface: ChromeUtils.generateQI([
    "nsIDOMGlobalPropertyInitializer",
    "nsISupportsWeakReference",
    "nsIObserver",
  ]),

  init(win) {
    console.debug("init()");

    this._window = win;

    this.initDOMRequestHelper(win);

    this._principal = win.document.nodePrincipal;

    try {
      this._topLevelPrincipal = win.top.document.nodePrincipal;
    } catch (error) {
      // Accessing the top-level document might fails if cross-origin
      this._topLevelPrincipal = undefined;
    }
  },

  __init(scope) {
    this._scope = scope;
  },

  askPermission() {
    console.debug("askPermission()");

    let hasValidTransientUserGestureActivation = this._window.document
      .hasValidTransientUserGestureActivation;

    return this.createPromise((resolve, reject) => {
      let permissionDenied = () => {
        reject(
          new this._window.DOMException(
            "User denied permission to use the Push API.",
            "NotAllowedError"
          )
        );
      };

      if (
        Services.prefs.getBoolPref("dom.push.testing.ignorePermission", false)
      ) {
        resolve();
        return;
      }

      this._requestPermission(
        hasValidTransientUserGestureActivation,
        resolve,
        permissionDenied
      );
    });
  },

  subscribe(options) {
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
        PushService.subscribeWithKey(
          this._scope,
          this._principal,
          keyView,
          callback
        );
      })
    );
  },

  _normalizeAppServerKey(appServerKey) {
    let key;
    if (typeof appServerKey == "string") {
      try {
        key = Cu.cloneInto(
          ChromeUtils.base64URLDecode(appServerKey, {
            padding: "reject",
          }),
          this._window
        );
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

  getSubscription() {
    console.debug("getSubscription()", this._scope);

    return this.createPromise((resolve, reject) => {
      let callback = new PushSubscriptionCallback(this, resolve, reject);
      PushService.getSubscription(this._scope, this._principal, callback);
    });
  },

  permissionState() {
    console.debug("permissionState()", this._scope);

    return this.createPromise((resolve, reject) => {
      let permission = Ci.nsIPermissionManager.UNKNOWN_ACTION;

      try {
        permission = this._testPermission();
      } catch (e) {
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

  _testPermission() {
    let permission = Services.perms.testExactPermissionFromPrincipal(
      this._principal,
      "desktop-notification"
    );
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

  _requestPermission(
    hasValidTransientUserGestureActivation,
    allowCallback,
    cancelCallback
  ) {
    // Create an array with a single nsIContentPermissionType element.
    let type = {
      type: "desktop-notification",
      options: [],
      QueryInterface: ChromeUtils.generateQI(["nsIContentPermissionType"]),
    };
    let typeArray = Cc["@mozilla.org/array;1"].createInstance(
      Ci.nsIMutableArray
    );
    typeArray.appendElement(type);

    // create a nsIContentPermissionRequest
    let request = {
      QueryInterface: ChromeUtils.generateQI(["nsIContentPermissionRequest"]),
      types: typeArray,
      principal: this._principal,
      hasValidTransientUserGestureActivation,
      topLevelPrincipal: this._topLevelPrincipal,
      allow: allowCallback,
      cancel: cancelCallback,
      window: this._window,
    };

    // Using askPermission from nsIDOMWindowUtils that takes care of the
    // remoting if needed.
    let windowUtils = this._window.windowUtils;
    windowUtils.askPermission(request);
  },
};

function PushSubscriptionCallback(pushManager, resolve, reject) {
  this.pushManager = pushManager;
  this.resolve = resolve;
  this.reject = reject;
}

PushSubscriptionCallback.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIPushSubscriptionCallback"]),

  onPushSubscription(ok, subscription) {
    let { pushManager } = this;
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
      p256dhKey,
      authSecret,
    };
    let appServerKey = this._getKey(subscription, "appServer");
    if (appServerKey) {
      // Avoid passing null keys to work around bug 1256449.
      options.appServerKey = appServerKey;
    }
    let sub = new pushManager._window.PushSubscription(options);
    this.resolve(sub);
  },

  _getKey(subscription, name) {
    let rawKey = Cu.cloneInto(
      subscription.getKey(name),
      this.pushManager._window
    );
    if (!rawKey.length) {
      return null;
    }

    let key = new this.pushManager._window.ArrayBuffer(rawKey.length);
    let keyView = new this.pushManager._window.Uint8Array(key);
    keyView.set(rawKey);
    return key;
  },

  _rejectWithError(result) {
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

const EXPORTED_SYMBOLS = ["Push"];
