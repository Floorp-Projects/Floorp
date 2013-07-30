/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function debug(aMsg) {
  // dump("-- InterAppMessagePort: " + Date.now() + ": " + aMsg + "\n");
}

function InterAppMessagePort() {
  debug("InterAppMessagePort()");
};

InterAppMessagePort.prototype = {
  classDescription: "MozInterAppMessagePort",

  classID: Components.ID("{c66e0f8c-e3cb-11e2-9e85-43ef6244b884}"),

  contractID: "@mozilla.org/dom/inter-app-message-port;1",

  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports]),

  postMessage: function(aMessage) {
    // TODO
  },

  start: function() {
    // TODO
  },

  close: function() {
    // TODO
  },

  get onmessage() {
    return this.__DOM_IMPL__.getEventHandler("onmessage");
  },

  set onmessage(aHandler) {
    this.__DOM_IMPL__.setEventHandler("onmessage", aHandler);
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([InterAppMessagePort]);

