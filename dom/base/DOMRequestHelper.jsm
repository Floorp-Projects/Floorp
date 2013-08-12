/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Helper object for APIs that deal with DOMRequests and Promises and need to
 * release them when the window goes out of scope.
 */
const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

this.EXPORTED_SYMBOLS = ["DOMRequestIpcHelper"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageListenerManager");

/**
 * We use DOMRequestIpcHelperMessageListener to avoid leaking objects which
 * "inherit" from DOMRequestIpcHelper.
 *
 * The issue is that the message manager will hold a strong ref to the message
 * listener we register with it.  But we don't want to hold a strong ref to the
 * DOMRequestIpcHelper object, because that object may be arbitrarily large.
 *
 * So instead the message manager holds a strong ref to the
 * DOMRequestIpcHelperMessageListener, and that holds a /weak/ ref to its
 * DOMRequestIpcHelper.
 *
 * Additionally, we want to unhook all of these message listeners when the
 * appropriate window is destroyed.  We use DOMRequestIpcHelperMessageListener
 * for this, too.
 */
this.DOMRequestIpcHelperMessageListener = function(aHelper, aWindow, aMessages) {
  this._weakHelper = Cu.getWeakReference(aHelper);

  this._messages = aMessages;
  this._messages.forEach(function(msgName) {
    cpmm.addWeakMessageListener(msgName, this);
  }, this);

  Services.obs.addObserver(this, "inner-window-destroyed", /* weakRef */ true);

  // aWindow may be null; in that case, the DOMRequestIpcHelperMessageListener
  // is not tied to a particular window and lives forever.
  if (aWindow) {
    let util = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIDOMWindowUtils);
    this._innerWindowID = util.currentInnerWindowID;
  }
}

DOMRequestIpcHelperMessageListener.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMessageListener,
                                         Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  observe: function(aSubject, aTopic, aData) {
    if (aTopic !== "inner-window-destroyed") {
      return;
    }

    let wId = aSubject.QueryInterface(Ci.nsISupportsPRUint64).data;
    if (wId != this._innerWindowID) {
      return;
    }

    this.destroy();
  },

  receiveMessage: function(aMsg) {
    let helper = this._weakHelper.get();
    if (helper) {
      helper.receiveMessage(aMsg);
    } else {
      this.destroy();
    }
  },

  destroy: function() {
    // DOMRequestIpcHelper.destroy() calls back into this function.
    if (this._destroyed) {
      return;
    }
    this._destroyed = true;

    Services.obs.removeObserver(this, "inner-window-destroyed");

    this._messages.forEach(function(msgName) {
      cpmm.removeWeakMessageListener(msgName, this);
    }, this);
    this._messages = null;

    let helper = this._weakHelper.get();
    if (helper) {
      helper.destroyDOMRequestHelper();
    }
  }
}

this.DOMRequestIpcHelper = function DOMRequestIpcHelper() {
}

DOMRequestIpcHelper.prototype = {
  /**
   * An object which "inherits" from DOMRequestIpcHelper and declares its own
   * queryInterface method MUST implement Ci.nsISupportsWeakReference.
   */
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupportsWeakReference]),

  initDOMRequestHelper: function(aWindow, aMessages) {
    this._DOMRequestIpcHelperMessageListener =
      new DOMRequestIpcHelperMessageListener(this, aWindow, aMessages);

    this._window = aWindow;
    this._requests = {};
    this._id = this._getRandomId();

    if (this._window) {
      // We don't use this.innerWindowID, but other classes rely on it.
      let util = this._window.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsIDOMWindowUtils);
      this.innerWindowID = util.currentInnerWindowID;
    }
  },

  getRequestId: function(aRequest) {
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
    if (this._requests[aId])
      return this._requests[aId];
  },

  getPromiseResolver: function(aId) {
    // Delegates to getRequest() since the lookup table is agnostic about
    // storage.
    return this.getRequest(aId);
  },

  removeRequest: function(aId) {
    if (this._requests[aId])
      delete this._requests[aId];
  },

  removePromiseResolver: function(aId) {
    // Delegates to getRequest() since the lookup table is agnostic about
    // storage.
    this.removeRequest(aId);
  },

  takeRequest: function(aId) {
    if (!this._requests[aId])
      return null;
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
    return Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator).generateUUID().toString();
  },

  destroyDOMRequestHelper: function() {
    // This function is re-entrant --
    // DOMRequestIpcHelperMessageListener.destroy() calls back into this
    // function, and this.uninit() may also call it.
    if (this._destroyed) {
      return;
    }
    this._destroyed = true;

    this._DOMRequestIpcHelperMessageListener.destroy();
    this._requests = {};
    this._window = null;

    if(this.uninit) {
      this.uninit();
    }
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
  }
}
