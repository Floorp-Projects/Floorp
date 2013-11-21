/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

XPCOMUtils.defineLazyServiceGetter(this, "appsService",
                                   "@mozilla.org/AppsService;1",
                                   "nsIAppsService");

const DEBUG = false;
function debug(aMsg) {
  dump("-- InterAppConnection: " + Date.now() + ": " + aMsg + "\n");
}

/**
 * MozInterAppConnection implementation.
 */

function InterAppConnection() {
  if (DEBUG) debug("InterAppConnection()");
  this.keyword = null;
  this.publisher = null;
  this.subscriber = null;
};

InterAppConnection.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  classDescription: "MozInterAppConnection",

  classID: Components.ID("{9dbfa904-0718-11e3-8e77-0721a45514b8}"),

  contractID: "@mozilla.org/dom/inter-app-connection;1",

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver]),

  __init: function(aKeyword, aPublisher, aSubscriber) {
    if (DEBUG) {
      debug("__init: aKeyword: " + aKeyword +
            " aPublisher: " + aPublisher + " aSubscriber: " + aSubscriber);
    }
    this.keyword = aKeyword;
    this.publisher = aPublisher;
    this.subscriber = aSubscriber;
  },

  // Ci.nsIDOMGlobalPropertyInitializer implementation.
  init: function(aWindow) {
    if (DEBUG) debug("init");

    this.initDOMRequestHelper(aWindow, []);
    let principal = aWindow.document.nodePrincipal;
    this._manifestURL = appsService.getManifestURLByLocalId(principal.appId);
  },

  cancel: function() {
    if (DEBUG) debug("cancel");

    cpmm.sendAsyncMessage("InterAppConnection:Cancel",
                          { keyword: this.keyword,
                            pubAppManifestURL: this.publisher,
                            subAppManifestURL: this.subscriber,
                            manifestURL: this._manifestURL });
  }
};


/**
 * MozInterAppConnectionRequest implementation.
 */

function InterAppConnectionRequest() {
  if (DEBUG) debug("InterAppConnectionRequest()");
  this.keyword = null;
  this.port = null;
};

InterAppConnectionRequest.prototype = {
  classDescription: "MozInterAppConnectionRequest",

  classID: Components.ID("{6a77e9e0-0645-11e3-b90b-73bb7c78e06a}"),

  contractID: "@mozilla.org/dom/inter-app-connection-request;1",

  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports]),

  __init: function(aKeyword, aPort) {
    if (DEBUG) debug("__init: aKeyword: " + aKeyword + " aPort: " + aPort);
    this.keyword = aKeyword;
    this.port = aPort;
  }
};

/**
 * InterAppConnectionRequestWrapper implementation.
 *
 * This implements nsISystemMessagesWrapper.wrapMessage(), which provides a
 * plugable way to wrap a "connection" type system message.
 *
 * Please see SystemMessageManager.js to know how it customizes the wrapper.
 */

function InterAppConnectionRequestWrapper() {
  if (DEBUG) debug("InterAppConnectionRequestWrapper()");
}

InterAppConnectionRequestWrapper.prototype = {
  // nsISystemMessagesWrapper implementation.
  wrapMessage: function(aMessage, aWindow) {
    if (DEBUG) debug("wrapMessage: " + JSON.stringify(aMessage));

    let port = new aWindow.MozInterAppMessagePort(aMessage.messagePortID);
    let connectionRequest =
      new aWindow.MozInterAppConnectionRequest(aMessage.keyword, port);

    return connectionRequest;
  },

  classDescription: "InterAppConnectionRequestWrapper",

  classID: Components.ID("{d7c7a466-f91d-11e2-812a-6fab12ece58e}"),

  contractID: "@mozilla.org/dom/system-messages/wrapper/connection;1",

  QueryInterface: XPCOMUtils.generateQI([Ci.nsISystemMessagesWrapper])
}


this.NSGetFactory =
  XPCOMUtils.generateNSGetFactory([InterAppConnection,
                                   InterAppConnectionRequest,
                                   InterAppConnectionRequestWrapper]);
