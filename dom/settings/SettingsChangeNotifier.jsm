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

const kXpcomShutdownObserverTopic      = "xpcom-shutdown";
const kMozSettingsChangedObserverTopic = "mozsettings-changed";
const kFromSettingsChangeNotifier      = "fromSettingsChangeNotifier";

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");

let SettingsChangeNotifier = {
  init: function() {
    debug("init");
    this.children = [];
    this._messages = ["Settings:Changed", "Settings:RegisterForMessages", "Settings:UnregisterForMessages"];
    this._messages.forEach((function(msgName) {
      ppmm.addMessageListener(msgName, this);
    }).bind(this));

    Services.obs.addObserver(this, kXpcomShutdownObserverTopic, false);
    Services.obs.addObserver(this, kMozSettingsChangedObserverTopic, false);
  },

  observe: function(aSubject, aTopic, aData) {
    debug("observe");
    switch (aTopic) {
      case kXpcomShutdownObserverTopic:
        this._messages.forEach((function(msgName) {
          ppmm.removeMessageListener(msgName, this);
        }).bind(this));
        Services.obs.removeObserver(this, kXpcomShutdownObserverTopic);
        Services.obs.removeObserver(this, kMozSettingsChangedObserverTopic);
        ppmm = null;
        break;
      case kMozSettingsChangedObserverTopic:
      {
        let setting = JSON.parse(aData);
        // To avoid redundantly broadcasting settings-changed events that are
        // just requested from content processes themselves, skip the observer
        // messages that are notified from the internal SettingsChangeNotifier.
        if (setting.message && setting.message === kFromSettingsChangeNotifier)
          return;
        this.broadcastMessage("Settings:Change:Return:OK",
          { key: setting.key, value: setting.value });
        break;
      }
      default:
        debug("Wrong observer topic: " + aTopic);
        break;
    }
  },

  broadcastMessage: function broadcastMessage(aMsgName, aContent) {
    let i;
    for (i = this.children.length - 1; i >= 0; i -= 1) {
      let msgMgr = this.children[i];
      try {
        msgMgr.sendAsyncMessage(aMsgName, aContent);
      } catch (e) {
        let index;
        if ((index = this.children.indexOf(msgMgr)) != -1) {
          this.children.splice(index, 1);
          dump("Remove dead MessageManager!\n");
        }
      }
    };
  },

  receiveMessage: function(aMessage) {
    debug("receiveMessage");
    let msg = aMessage.json;
    let mm = aMessage.target;
    switch (aMessage.name) {
      case "Settings:Changed":
        this.broadcastMessage("Settings:Change:Return:OK",
          { key: msg.key, value: msg.value });
        Services.obs.notifyObservers(this, kMozSettingsChangedObserverTopic,
          JSON.stringify({
            key: msg.key,
            value: msg.value,
            message: kFromSettingsChangeNotifier
          }));
        break;
      case "Settings:RegisterForMessages":
        debug("Register!");
        if (this.children.indexOf(mm) == -1) {
          this.children.push(mm);
        }
        break;
      case "Settings:UnregisterForMessages":
        debug("Unregister");
        let index;
        if ((index = this.children.indexOf(mm)) != -1) {
          this.children.splice(index, 1);
        }
        break;
      default:
        debug("Wrong message: " + aMessage.name);
    }
  }
}

SettingsChangeNotifier.init();
