/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Helper object for APIs that deal with DOMRequests and Promises.
 * It allows objects inheriting from it to create and keep track of DOMRequests
 * and Promises objects in the common scenario where requests are created in
 * the child, handed out to content and delivered to the parent within an async
 * message (containing the identifiers of these requests). The parent may send
 * messages back as answers to different requests and the child will use this
 * helper to get the right request object. This helper also takes care of
 * releasing the requests objects when the window goes out of scope.
 *
 * DOMRequestIPCHelper also deals with message listeners, allowing to add them
 * to the child side of frame and process message manager and removing them
 * when needed.
 */
const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

this.EXPORTED_SYMBOLS = ["DOMRequestIpcHelper"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageListenerManager");

this.DOMRequestIpcHelper = function DOMRequestIpcHelper() {
  // _listeners keeps a list of messages for which we added a listener and the
  // kind of listener that we added (strong or weak). It's an object of this
  // form:
  //  {
  //    "message1": true,
  //    "messagen": false
  //  }
  //
  // where each property is the name of the message and its value is a boolean
  // that indicates if the listener is strong or not.
  this._listeners = null;
  this._requests = null;
  this._window = null;
}

DOMRequestIpcHelper.prototype = {
  /**
   * An object which "inherits" from DOMRequestIpcHelper, declares its own
   * queryInterface method and adds at least one weak listener to the Message
   * Manager MUST implement Ci.nsISupportsWeakReference.
   */
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver]),

   /**
   *  'aMessages' is expected to be an array of either:
   *  - objects of this form:
   *    {
   *      name: "messageName",
   *      strongRef: false
   *    }
   *    where 'name' is the message identifier and 'strongRef' a boolean
   *    indicating if the listener should be a strong referred one or not.
   *
   *  - or only strings containing the message name, in which case the listener
   *    will be added as a weak reference by default.
   */
  addMessageListeners: function(aMessages) {
    if (!aMessages) {
      return;
    }

    if (!this._listeners) {
      this._listeners = {};
    }

    if (!Array.isArray(aMessages)) {
      aMessages = [aMessages];
    }

    aMessages.forEach((aMsg) => {
      let name = aMsg.name || aMsg;
      // If the listener is already set and it is of the same type we just
      // bail out. If it is not of the same type, we throw an exception.
      if (this._listeners[name] != undefined) {
        if (!!aMsg.strongRef == this._listeners[name]) {
          return;
        } else {
          throw Cr.NS_ERROR_FAILURE;
        }
      }

      aMsg.strongRef ? cpmm.addMessageListener(name, this)
                     : cpmm.addWeakMessageListener(name, this);
      this._listeners[name] = !!aMsg.strongRef;
    });
  },

  /**
   * 'aMessages' is expected to be a string or an array of strings containing
   * the message names of the listeners to be removed.
   */
  removeMessageListeners: function(aMessages) {
    if (!this._listeners || !aMessages) {
      return;
    }

    if (!Array.isArray(aMessages)) {
      aMessages = [aMessages];
    }

    aMessages.forEach((aName) => {
      if (this._listeners[aName] == undefined) {
        return;
      }

      this._listeners[aName] ? cpmm.removeMessageListener(aName, this)
                             : cpmm.removeWeakMessageListener(aName, this);
      delete this._listeners[aName];
    });
  },

  /**
   * Initialize the helper adding the corresponding listeners to the messages
   * provided as the second parameter.
   *
   * 'aMessages' is expected to be an array of either:
   *
   *  - objects of this form:
   *    {
   *      name: 'messageName',
   *      strongRef: false
   *    }
   *    where 'name' is the message identifier and 'strongRef' a boolean
   *    indicating if the listener should be a strong referred one or not.
   *
   *  - or only strings containing the message name, in which case the listener
   *    will be added as a weak referred one by default.
   */
  initDOMRequestHelper: function(aWindow, aMessages) {
    // Query our required interfaces to force a fast fail if they are not
    // provided. These calls will throw if the interface is not available.
    this.QueryInterface(Ci.nsISupportsWeakReference);
    this.QueryInterface(Ci.nsIObserver);

    if (aMessages) {
      this.addMessageListeners(aMessages);
    }

    this._id = this._getRandomId();

    this._window = aWindow;
    if (this._window) {
      // We don't use this.innerWindowID, but other classes rely on it.
      let util = this._window.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsIDOMWindowUtils);
      this.innerWindowID = util.currentInnerWindowID;
    }

    this._destroyed = false;

    Services.obs.addObserver(this, "inner-window-destroyed",
                             /* weak-ref */ true);
  },

  destroyDOMRequestHelper: function() {
    if (this._destroyed) {
      return;
    }

    this._destroyed = true;

    Services.obs.removeObserver(this, "inner-window-destroyed");

    if (this._listeners) {
      Object.keys(this._listeners).forEach((aName) => {
        this._listeners[aName] ? cpmm.removeMessageListener(aName, this)
                               : cpmm.removeWeakMessageListener(aName, this);
        delete this._listeners[aName];
      });
    }

    this._listeners = null;
    this._requests = null;

    // Objects inheriting from DOMRequestIPCHelper may have an uninit function.
    if (this.uninit) {
      this.uninit();
    }

    this._window = null;
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic !== "inner-window-destroyed") {
      return;
    }

    let wId = aSubject.QueryInterface(Ci.nsISupportsPRUint64).data;
    if (wId != this.innerWindowID) {
      return;
    }

    this.destroyDOMRequestHelper();
  },

  getRequestId: function(aRequest) {
    if (!this._requests) {
      this._requests = {};
    }

    let id = "id" + this._getRandomId();
    this._requests[id] = aRequest;
    return id;
  },

  getPromiseResolverId: function(aPromiseResolver) {
    // Delegates to getRequest() since the lookup table is agnostic about
    // storage.
    return this.getRequestId(aPromiseResolver);
  },

  getRequest: function(aId) {
    if (this._requests && this._requests[aId]) {
      return this._requests[aId];
    }
  },

  getPromiseResolver: function(aId) {
    // Delegates to getRequest() since the lookup table is agnostic about
    // storage.
    return this.getRequest(aId);
  },

  removeRequest: function(aId) {
    if (this._requests && this._requests[aId]) {
      delete this._requests[aId];
    }
  },

  removePromiseResolver: function(aId) {
    // Delegates to getRequest() since the lookup table is agnostic about
    // storage.
    this.removeRequest(aId);
  },

  takeRequest: function(aId) {
    if (!this._requests || !this._requests[aId]) {
      return null;
    }
    let request = this._requests[aId];
    delete this._requests[aId];
    return request;
  },

  takePromiseResolver: function(aId) {
    // Delegates to getRequest() since the lookup table is agnostic about
    // storage.
    return this.takeRequest(aId);
  },

  _getRandomId: function() {
    return Cc["@mozilla.org/uuid-generator;1"]
             .getService(Ci.nsIUUIDGenerator).generateUUID().toString();
  },

  createRequest: function() {
    return Services.DOMRequest.createRequest(this._window);
  },

  /**
   * createPromise() creates a new Promise, with `aPromiseInit` as the
   * PromiseInit callback. The promise constructor is obtained from the
   * reference to window owned by this DOMRequestIPCHelper.
   */
  createPromise: function(aPromiseInit) {
    return new this._window.Promise(aPromiseInit);
  },

  forEachRequest: function(aCallback) {
    if (!this._requests) {
      return;
    }

    Object.keys(this._requests).forEach((aKey) => {
      if (this.getRequest(aKey) instanceof this._window.DOMRequest) {
        aCallback(aKey);
      }
    });
  },

  forEachPromiseResolver: function(aCallback) {
    if (!this._requests) {
      return;
    }

    Object.keys(this._requests).forEach((aKey) => {
      if ("resolve" in this.getPromiseResolver(aKey) &&
          "reject" in this.getPromiseResolver(aKey)) {
        aCallback(aKey);
      }
    });
  },
}
