/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

const DEBUG = false;

const TETHERING_TYPE_WIFI       = "wifi";
const TETHERING_TYPE_BLUETOOTH  = "bt";
const TETHERING_TYPE_USB        = "usb";

function TetheringManager() {
}

TetheringManager.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  classDescription: "TetheringManager",
  classID: Components.ID("{bd8a831c-d8ec-4f00-8803-606e50781097}"),
  contractID: "@mozilla.org/dom/tetheringmanager;1",

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver]),

  init: function(aWindow) {
    const messages = ["WifiManager:setWifiTethering:Return:OK",
                      "WifiManager:setWifiTethering:Return:NO"];
    this.initDOMRequestHelper(aWindow, messages);
  },

  // TODO : aMessage format may be different after supporting bt/usb.
  //        for now, use wifi format first.
  receiveMessage: function(aMessage) {
    let data = aMessage.data.data;

    let resolver = this.takePromiseResolver(data.resolverId);
    if (!resolver) {
      return;
    }

    switch (aMessage.name) {
      case "WifiManager:setWifiTethering:Return:OK":
        resolver.resolve(data);
        break;
      case "WifiManager:setWifiTethering:Return:NO":
        resolver.reject(data.reason);
        break;
    }
  },

  setTetheringEnabled: function setTetheringEnabled(aEnabled, aType, aConfig) {
    let self = this;
    switch (aType) {
      case TETHERING_TYPE_WIFI:
        return this.createPromiseWithId(function(aResolverId) {
          let data = { resolverId: aResolverId, enabled: aEnabled, config: aConfig };
          cpmm.sendAsyncMessage("WifiManager:setWifiTethering", { data: data});
        });
      case TETHERING_TYPE_BLUETOOTH:
      case TETHERING_TYPE_USB:
      default:
        debug("tethering type(" + aType + ") doesn't support");
        return this.createPromiseWithId(function(aResolverId) {
          self.takePromiseResolver(aResolverId).reject();
        });
    }
  },
};

this.NSGetFactory =
  XPCOMUtils.generateNSGetFactory([TetheringManager]);

var debug;
if (DEBUG) {
  debug = function (s) {
    dump("-*- TetheringManager component: " + s + "\n");
  };
} else {
  debug = function (s) {};
}
