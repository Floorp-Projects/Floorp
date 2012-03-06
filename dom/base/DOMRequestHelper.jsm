/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
  * helper object for APIs that deal with DOMRequest and need to release them properly
  * when the window goes out of scope
  */
const Cu = Components.utils; 
const Cc = Components.classes;
const Ci = Components.interfaces;

let EXPORTED_SYMBOLS = ["DOMRequestIpcHelper"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(Services, "rs", function() {
  return Cc["@mozilla.org/dom/dom-request-service;1"].getService(Ci.nsIDOMRequestService);
});

XPCOMUtils.defineLazyGetter(this, "cpmm", function() {
  return Cc["@mozilla.org/childprocessmessagemanager;1"].getService(Ci.nsIFrameMessageManager);
});

function DOMRequestIpcHelper() {
}

DOMRequestIpcHelper.prototype = {
  getRequestId: function(aRequest) {
    let id = "id" + this._getRandomId();
    this._requests[id] = aRequest;
    return id;
  },

  getRequest: function(aId) {
    if (this._requests[aId])
      return this._requests[aId];
  },

  removeRequest: function(aId) {
    if (this._requests[aId])
      delete this._requests[aId];
  },

  _getRandomId: function() {
    return Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator).generateUUID().toString();
  },

  observe: function(aSubject, aTopic, aData) {
    let wId = aSubject.QueryInterface(Ci.nsISupportsPRUint64).data;
    if (wId == this.innerWindowID) {
      Services.obs.removeObserver(this, "inner-window-destroyed");
      this._requests = [];
      this._window = null;
      this._messages.forEach((function(msgName) {
        cpmm.removeMessageListener(msgName, this);
      }).bind(this));
      if(this.uninit)
        this.uninit();
    }
  },

  initHelper: function(aWindow, aMessages) {
    this._messages = aMessages;
    this._requests = [];
    this._window = aWindow;
    let util = this._window.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    this.innerWindowID = util.currentInnerWindowID;
    this._id = this._getRandomId();
    Services.obs.addObserver(this, "inner-window-destroyed", false);
    this._messages.forEach((function(msgName) {
      cpmm.addMessageListener(msgName, this);
    }).bind(this));
  },

  createRequest: function() {
    return Services.rs.createRequest(this._window);
  }
}
