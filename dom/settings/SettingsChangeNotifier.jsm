/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

function debug(s) {
//  dump("-*- SettingsChangeNotifier: " + s + "\n");
}

const Cu = Components.utils; 
const Cc = Components.classes;
const Ci = Components.interfaces;

let EXPORTED_SYMBOLS = ["SettingsChangeNotifier"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "ppmm", function() {
  return Cc["@mozilla.org/parentprocessmessagemanager;1"].getService(Ci.nsIFrameMessageManager);
});


let SettingsChangeNotifier = {
  init: function() {
    debug("init");
    ppmm.addMessageListener("Settings:Changed", this);
    Services.obs.addObserver(this, "xpcom-shutdown", false);
  },

  observe: function(aSubject, aTopic, aData) {
    debug("observe");
    ppmm.removeMessageListener("Settings:Changed", this);
    Services.obs.removeObserver(this, "xpcom-shutdown");
    ppmm = null;
  },

  receiveMessage: function(aMessage) {
    debug("receiveMessage");
    let msg = aMessage.json;
    switch (aMessage.name) {
      case "Settings:Changed":
        ppmm.sendAsyncMessage("Settings:Change:Return:OK", { key: msg.key, value: msg.value });
        Services.obs.notifyObservers(this, "mozsettings-changed", JSON.stringify({
          key: msg.key,
          value: msg.value
        }));
        break;
      default: 
        debug("Wrong message: " + aMessage.name);
    }
  }
}

SettingsChangeNotifier.init();
