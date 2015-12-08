/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This file exports XPCOM components for C++ and chrome JavaScript callers to
 * interact with the Push service.
 */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

var isParent = Services.appinfo.processType === Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;

/**
 * `PushServiceBase`, `PushServiceParent`, and `PushServiceContent` collectively
 * implement the `nsIPushService` interface. This interface provides calls
 * similar to the Push DOM API, but does not require service workers.
 *
 * Push service methods may be called from the parent or content process. The
 * parent process implementation loads `PushService.jsm` at app startup, and
 * calls its methods directly. The content implementation forwards calls to
 * the parent Push service via IPC.
 *
 * The implementations share a class and contract ID.
 */
function PushServiceBase() {
  this.wrappedJSObject = this;
  this._addListeners();
}

PushServiceBase.prototype = {
  classID: Components.ID("{daaa8d73-677e-4233-8acd-2c404bd01658}"),
  contractID: "@mozilla.org/push/Service;1",
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
    Ci.nsIPushService,
    Ci.nsIPushQuotaManager,
  ]),

  _handleReady() {},

  _addListeners() {
    for (let message of this._messages) {
      this._mm.addMessageListener(message, this);
    }
  },

  _isValidMessage(message) {
    return this._messages.includes(message.name);
  },

  observe(subject, topic, data) {
    if (topic === "app-startup") {
      Services.obs.addObserver(this, "sessionstore-windows-restored", true);
      return;
    }
    if (topic === "sessionstore-windows-restored") {
      Services.obs.removeObserver(this, "sessionstore-windows-restored");
      this._handleReady();
      return;
    }
  },

  _deliverSubscription(request, props) {
    if (!props) {
      request.onPushSubscription(Cr.NS_OK, null);
      return;
    }
    request.onPushSubscription(Cr.NS_OK, new PushSubscription(props));
  },
};

/**
 * The parent process implementation of `nsIPushService`. This version loads
 * `PushService.jsm` at startup and calls its methods directly. It also
 * receives and responds to requests from the content process.
 */
function PushServiceParent() {
  PushServiceBase.call(this);
}

PushServiceParent.prototype = Object.create(PushServiceBase.prototype);

XPCOMUtils.defineLazyServiceGetter(PushServiceParent.prototype, "_mm",
  "@mozilla.org/parentprocessmessagemanager;1", "nsIMessageBroadcaster");

XPCOMUtils.defineLazyGetter(PushServiceParent.prototype, "_service",
  function() {
    const {PushService} = Cu.import("resource://gre/modules/PushService.jsm",
                                    {});
    PushService.init();
    return PushService;
});

Object.assign(PushServiceParent.prototype, {
  _xpcom_factory: XPCOMUtils.generateSingletonFactory(PushServiceParent),

  _messages: [
    "Push:Register",
    "Push:Registration",
    "Push:Unregister",
    "Push:Clear",
    "Push:RegisterEventNotificationListener",
    "Push:NotificationForOriginShown",
    "Push:NotificationForOriginClosed",
    "child-process-shutdown",
  ],

  // nsIPushService methods

  subscribe(scope, principal, callback) {
    return this._handleRequest("Push:Register", principal, {
      scope: scope,
    }).then(result => {
      this._deliverSubscription(callback, result);
    }, error => {
      callback.onPushSubscription(Cr.NS_ERROR_FAILURE, null);
    }).catch(Cu.reportError);
  },

  unsubscribe(scope, principal, callback) {
    this._handleRequest("Push:Unregister", principal, {
      scope: scope,
    }).then(result => {
      callback.onUnsubscribe(Cr.NS_OK, result);
    }, error => {
      callback.onUnsubscribe(Cr.NS_ERROR_FAILURE, false);
    }).catch(Cu.reportError);
  },

  getSubscription(scope, principal, callback) {
    return this._handleRequest("Push:Registration", principal, {
      scope: scope,
    }).then(result => {
      this._deliverSubscription(callback, result);
    }, error => {
      callback.onPushSubscription(Cr.NS_ERROR_FAILURE, null);
    }).catch(Cu.reportError);
  },

  clearForDomain(domain, callback) {
    let principal = Services.scriptSecurityManager.getSystemPrincipal();
    return this._handleRequest("Push:Clear", principal, {
      domain: domain,
    }).then(result => {
      callback.onClear(Cr.NS_OK);
    }, error => {
      callback.onClear(Cr.NS_ERROR_FAILURE);
    }).catch(Cu.reportError);
  },

  // nsIPushQuotaManager methods

  notificationForOriginShown(origin) {
    this._service.notificationForOriginShown(origin);
  },

  notificationForOriginClosed(origin) {
    this._service.notificationForOriginClosed(origin);
  },

  receiveMessage(message) {
    if (!this._isValidMessage(message)) {
      return;
    }
    let {name, principal, target, data} = message;
    if (name === "Push:RegisterEventNotificationListener") {
      this._service.registerListener(target);
      return;
    }
    if (name === "child-process-shutdown") {
      this._service.unregisterListener(target);
      return;
    }
    if (name === "Push:NotificationForOriginShown") {
      this.notificationForOriginShown(data);
      return;
    }
    if (name === "Push:NotificationForOriginClosed") {
      this.notificationForOriginClosed(data);
      return;
    }
    if (!target.assertPermission("push")) {
      return;
    }
    let sender = target.QueryInterface(Ci.nsIMessageSender);
    return this._handleRequest(name, principal, data).then(result => {
      sender.sendAsyncMessage(this._getResponseName(name, "OK"), {
        requestID: data.requestID,
        result: result
      });
    }, error => {
      sender.sendAsyncMessage(this._getResponseName(name, "KO"), {
        requestID: data.requestID,
      });
    }).catch(Cu.reportError);
  },

  _handleReady() {
    this._service.init();
  },

  _toPageRecord(principal, data) {
    if (!data.scope) {
      throw new Error("Invalid page record: missing scope");
    }

    data.originAttributes =
      ChromeUtils.originAttributesToSuffix(principal.originAttributes);

    return data;
  },

  _handleRequest(name, principal, data) {
    if (!principal) {
      return Promise.reject(new Error("Invalid request: missing principal"));
    }

    if (name == "Push:Clear") {
      return this._service.clear(data);
    }

    let pageRecord;
    try {
      pageRecord = this._toPageRecord(principal, data);
    } catch (e) {
      return Promise.reject(e);
    }

    if (name === "Push:Register") {
      return this._service.register(pageRecord);
    }
    if (name === "Push:Registration") {
      return this._service.registration(pageRecord);
    }
    if (name === "Push:Unregister") {
      return this._service.unregister(pageRecord);
    }

    return Promise.reject(new Error("Invalid request: unknown name"));
  },

  _getResponseName(requestName, suffix) {
    let name = requestName.slice("Push:".length);
    return "PushService:" + name + ":" + suffix;
  },
});

/**
 * The content process implementation of `nsIPushService`. This version
 * uses the child message manager to forward calls to the parent process.
 * The parent Push service instance handles the request, and responds with a
 * message containing the result.
 */
function PushServiceContent() {
  PushServiceBase.apply(this, arguments);
  this._requests = new Map();
  this._requestId = 0;
}

PushServiceContent.prototype = Object.create(PushServiceBase.prototype);

XPCOMUtils.defineLazyServiceGetter(PushServiceContent.prototype,
  "_mm", "@mozilla.org/childprocessmessagemanager;1",
  "nsISyncMessageSender");

Object.assign(PushServiceContent.prototype, {
  _xpcom_factory: XPCOMUtils.generateSingletonFactory(PushServiceContent),

  _messages: [
    "PushService:Register:OK",
    "PushService:Register:KO",
    "PushService:Registration:OK",
    "PushService:Registration:KO",
    "PushService:Unregister:OK",
    "PushService:Unregister:KO",
    "PushService:Clear:OK",
    "PushService:Clear:KO",
  ],

  // nsIPushService methods

  subscribe(scope, principal, callback) {
    let requestId = this._addRequest(callback);
    this._mm.sendAsyncMessage("Push:Register", {
      scope: scope,
      requestID: requestId,
    }, null, principal);
  },

  unsubscribe(scope, principal, callback) {
    let requestId = this._addRequest(callback);
    this._mm.sendAsyncMessage("Push:Unregister", {
      scope: scope,
      requestID: requestId,
    }, null, principal);
  },

  getSubscription(scope, principal, callback) {
    let requestId = this._addRequest(callback);
    this._mm.sendAsyncMessage("Push:Registration", {
      scope: scope,
      requestID: requestId,
    }, null, principal);
  },

  clearForDomain(domain, callback) {
    let requestId = this._addRequest(callback);
    this._mm.sendAsyncMessage("Push:Clear", {
      domain: domain,
      requestID: requestId,
    });
  },

  // nsIPushQuotaManager methods

  notificationForOriginShown(origin) {
    this._mm.sendAsyncMessage("Push:NotificationForOriginShown", origin);
  },

  notificationForOriginClosed(origin) {
    this._mm.sendAsyncMessage("Push:NotificationForOriginClosed", origin);
  },

  _addRequest(data) {
    let id = ++this._requestId;
    this._requests.set(id, data);
    return id;
  },

  _takeRequest(requestId) {
    let d = this._requests.get(requestId);
    this._requests.delete(requestId);
    return d;
  },

  receiveMessage(message) {
    if (!this._isValidMessage(message)) {
      return;
    }
    let {name, data} = message;
    let request = this._takeRequest(data.requestID);

    if (!request) {
      return;
    }

    switch (name) {
      case "PushService:Register:OK":
      case "PushService:Registration:OK":
        this._deliverSubscription(request, data.result);
        break;

      case "PushService:Register:KO":
      case "PushService:Registration:KO":
        request.onPushSubscription(Cr.NS_ERROR_FAILURE, null);
        break;

      case "PushService:Unregister:OK":
        if (typeof data.result === "boolean") {
          request.onUnsubscribe(Cr.NS_OK, data.result);
        } else {
          request.onUnsubscribe(Cr.NS_ERROR_FAILURE, false);
        }
        break;

      case "PushService:Unregister:KO":
        request.onUnsubscribe(Cr.NS_ERROR_FAILURE, false);
        break;

      case "PushService:Clear:OK":
        request.onClear(Cr.NS_OK);
        break;

      case "PushService:Clear:KO":
        request.onClear(Cr.NS_ERROR_FAILURE);
        break;

      default:
        break;
    }
  },
});

/** `PushSubscription` instances are passed to all subscription callbacks. */
function PushSubscription(props) {
  this._props = props;
}

PushSubscription.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPushSubscription]),

  /** The URL for sending messages to this subscription. */
  get endpoint() {
    return this._props.endpoint;
  },

  /** The last time a message was sent to this subscription. */
  get lastPush() {
    return this._props.lastPush;
  },

  /** The total number of messages sent to this subscription. */
  get pushCount() {
    return this._props.pushCount;
  },

  /** The number of remaining background messages that can be sent to this
   * subscription, or -1 of the subscription is exempt from the quota.
   */
  get quota() {
    return this._props.quota;
  },

  /**
   * Indicates whether this subscription is subject to the background message
   * quota.
   */
  quotaApplies() {
    return this.quota >= 0;
  },

  /**
   * Indicates whether this subscription exceeded the background message quota,
   * or the user revoked the notification permission. The caller must request a
   * new subscription to continue receiving push messages.
   */
  isExpired() {
    return this.quota === 0;
  },

  /**
   * Returns a key for encrypting messages sent to this subscription. JS
   * callers receive the key buffer as a return value, while C++ callers
   * receive the key size and buffer as out parameters.
   */
  getKey(name, outKeyLen) {
    if (name === "p256dh") {
      return this._getRawKey(this._props.p256dhKey, outKeyLen);
    }
    if (name === "auth") {
      return this._getRawKey(this._props.authenticationSecret, outKeyLen);
    }
    return null;
  },

  _getRawKey(key, outKeyLen) {
    if (!key) {
      return null;
    }
    let rawKey = new Uint8Array(key);
    if (outKeyLen) {
      outKeyLen.value = rawKey.length;
    }
    return rawKey;
  },
};

/**
 * `PushObserverNotification` instances are passed to all
 * `push-notification` observers.
 */
function PushObserverNotification() {}

PushObserverNotification.prototype = {
  classID: Components.ID("{e68997fd-8b92-49ee-af12-800830b023e8}"),
  contractID: "@mozilla.org/push/ObserverNotification;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPushObserverNotification]),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([
  PushObserverNotification,

  // Export the correct implementation depending on whether we're running in
  // the parent or content process.
  isParent ? PushServiceParent : PushServiceContent,
]);
