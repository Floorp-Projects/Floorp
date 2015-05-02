/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Don't modify this, instead set dom.push.debug.
let gDebuggingEnabled = true;

function debug(s) {
  if (gDebuggingEnabled)
    dump("-*- Push.js: " + s + "\n");
}

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");

const PUSH_SUBSCRIPTION_CID = Components.ID("{CA86B665-BEDA-4212-8D0F-5C9F65270B58}");

function PushSubscription(pushEndpoint, scope, pageURL) {
  debug("PushSubscription Constructor");
  this._pushEndpoint = pushEndpoint;
  this._scope = scope;
  this._pageURL = pageURL;
}

PushSubscription.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  contractID: "@mozilla.org/push/PushSubscription;1",

  classID : PUSH_SUBSCRIPTION_CID,

  QueryInterface : XPCOMUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer,
                                          Ci.nsISupportsWeakReference,
                                          Ci.nsIObserver]),

  init: function(aWindow) {
    debug("PushSubscription init()");

    this.initDOMRequestHelper(aWindow, [
      "PushService:Unregister:OK",
      "PushService:Unregister:KO",
    ]);

    this._cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"]
                   .getService(Ci.nsISyncMessageSender);
  },

  __init: function(endpoint, scope, pageURL) {
    this._pushEndpoint = endpoint;
    this._scope = scope;
    this._pageURL = pageURL;
  },

  get endpoint() {
    return this._pushEndpoint;
  },

  unsubscribe: function() {
    debug("unsubscribe! ")

    let promiseInit = function(resolve, reject) {
      let resolverId = this.getPromiseResolverId({resolve: resolve,
                                                  reject: reject });

      this._cpmm.sendAsyncMessage("Push:Unregister", {
                                  pageURL: this._pageURL,
                                  scope: this._scope,
                                  pushEndpoint: this._pushEndpoint,
                                  requestID: resolverId
                                });
    }.bind(this);

    return this.createPromise(promiseInit);
  },

  receiveMessage: function(aMessage) {
    debug("push subscription receiveMessage(): " + JSON.stringify(aMessage))

    let json = aMessage.data;
    let resolver = this.takePromiseResolver(json.requestID);
    if (resolver == null) {
      return;
    }

    switch (aMessage.name) {
      case "PushService:Unregister:OK":
        resolver.resolve(false);
        break;
      case "PushService:Unregister:KO":
        resolver.reject(true);
        break;
      default:
        debug("NOT IMPLEMENTED! receiveMessage for " + aMessage.name);
    }
  },

};

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

    this._pageURL = aWindow.document.nodePrincipal.URI;
    this._window = aWindow;

    this.initDOMRequestHelper(aWindow, [
      "PushService:Register:OK",
      "PushService:Register:KO",
      "PushService:Registration:OK",
      "PushService:Registration:KO"
    ]);

    this._cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"]
                   .getService(Ci.nsISyncMessageSender);
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

    debug("Existing permission " + permValue);

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

    debug("asking the window utils about permission...")
    // Using askPermission from nsIDOMWindowUtils that takes care of the
    // remoting if needed.
    let windowUtils = this._window.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDOMWindowUtils);
    windowUtils.askPermission(request);
  },



  receiveMessage: function(aMessage) {
    debug("push receiveMessage(): " + JSON.stringify(aMessage))

    let json = aMessage.data;
    let resolver = this.takePromiseResolver(json.requestID);

    if (!resolver) {
      return;
    }

    switch (aMessage.name) {
      case "PushService:Register:OK":
      {
        let subscription = new this._window.PushSubscription(json.pushEndpoint,
                                                             this._scope,
                                                             this._pageURL.spec);
        resolver.resolve(subscription);
        break;
      }
      case "PushService:Register:KO":
        resolver.reject(null);
        break;
      case "PushService:Registration:OK":
      {
        let subscription = null;
        try {
          subscription = new this._window.PushSubscription(json.registration.pushEndpoint,
                                                          this._scope, this._pageURL.spec);
        } catch(error) {
        }
        resolver.resolve(subscription);
        break;
      }
      case "PushService:Registration:KO":
        resolver.reject(null);
        break;
      default:
        debug("NOT IMPLEMENTED! receiveMessage for " + aMessage.name);
    }
  },

  subscribe: function() {
    debug("subscribe()");
    let p = this.createPromise(function(resolve, reject) {
      let resolverId = this.getPromiseResolverId({ resolve: resolve, reject: reject });

      this.askPermission(
        function() {
          this._cpmm.sendAsyncMessage("Push:Register", {
                                      pageURL: this._pageURL.spec,
                                      scope: this._scope,
                                      requestID: resolverId
                                    });
        }.bind(this),

        function() {
          reject("PermissionDeniedError");
        }
      );
    }.bind(this));
    return p;
  },

  getSubscription: function() {
    debug("getSubscription()" + this._scope);

    let p = this.createPromise(function(resolve, reject) {

      let resolverId = this.getPromiseResolverId({ resolve: resolve, reject: reject });

      this.askPermission(
        function() {
          this._cpmm.sendAsyncMessage("Push:Registration", {
                                      pageURL: this._pageURL.spec,
                                      scope: this._scope,
                                      requestID: resolverId
                                    });
        }.bind(this),

        function() {
          reject("PermissionDeniedError");
        }
      );
    }.bind(this));
    return p;
  },

  hasPermission: function() {
    debug("getSubscription()" + this._scope);

    let p = this.createPromise(function(resolve, reject) {
      let permissionManager = Cc["@mozilla.org/permissionmanager;1"].getService(Ci.nsIPermissionManager);
      let permission = permissionManager.testExactPermission(this._pageURL, "push");

      let pushPermissionStatus = "default";
      if (permission == Ci.nsIPermissionManager.ALLOW_ACTION) {
        pushPermissionStatus = "granted";
      } else if (permission == Ci.nsIPermissionManager.DENY_ACTION) {
        pushPermissionStatus = "denied";
      }
      resolve(pushPermissionStatus);
    }.bind(this));
    return p;
  },
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([Push, PushSubscription]);
