/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

const DEBUG = false;
function debug(s) {
  if (DEBUG) dump("-*- SettingsChangeNotifier: " + s + "\n");
}

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

this.EXPORTED_SYMBOLS = ["SettingsChangeNotifier"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const kXpcomShutdownObserverTopic      = "xpcom-shutdown";
const kMozSettingsChangedObserverTopic = "mozsettings-changed";
const kFromSettingsChangeNotifier      = "fromSettingsChangeNotifier";

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");

this.SettingsChangeNotifier = {
  init: function() {
    if (DEBUG) debug("init");
    this.children = [];
    this._messages = ["Settings:Changed", "Settings:RegisterForMessages", "child-process-shutdown"];
    this._messages.forEach((function(msgName) {
      ppmm.addMessageListener(msgName, this);
    }).bind(this));

    Services.obs.addObserver(this, kXpcomShutdownObserverTopic, false);
    Services.obs.addObserver(this, kMozSettingsChangedObserverTopic, false);

    // Broadcast when SettingChangeNotifier finishes init process.
    ppmm.broadcastAsyncMessage("Settings:Notifier:Init:OK");
  },

  observe: function(aSubject, aTopic, aData) {
    if (DEBUG) debug("observe");
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
        if (DEBUG) debug("Wrong observer topic: " + aTopic);
        break;
    }
  },

  broadcastMessage: function broadcastMessage(aMsgName, aContent) {
    if (DEBUG) debug("Broadast");
    this.children.forEach(function(msgMgr) {
      msgMgr.sendAsyncMessage(aMsgName, aContent);
    });
  },

  receiveMessage: function(aMessage) {
    if (DEBUG) debug("receiveMessage");
    let msg = aMessage.data;
    let mm = aMessage.target;
    switch (aMessage.name) {
      case "Settings:Changed":
        if (!aMessage.target.assertPermission("settings-write")) {
          Cu.reportError("Settings message " + msg.name +
                         " from a content process with no 'settings-write' privileges.");
          return null;
        }
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
        if (!aMessage.target.assertPermission("settings-read")) {
          Cu.reportError("Settings message " + msg.name +
                         " from a content process with no 'settings-read' privileges.");
          return null;
        }
        if (DEBUG) debug("Register!");
        if (this.children.indexOf(mm) == -1) {
          this.children.push(mm);
        }
        break;
      case "child-process-shutdown":
        if (DEBUG) debug("Unregister");
        let index;
        if ((index = this.children.indexOf(mm)) != -1) {
          if (DEBUG) debug("Unregister index: " + index);
          this.children.splice(index, 1);
        }
        break;
      default:
        if (DEBUG) debug("Wrong message: " + aMessage.name);
    }
  }
}

SettingsChangeNotifier.init();
