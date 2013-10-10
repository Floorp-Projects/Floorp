/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function debug(msg) {
  Services.console.logStringMessage("SessionStoreContent: " + msg);
}

let Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);

XPCOMUtils.defineLazyModuleGetter(this, "DocShellCapabilities",
  "resource:///modules/sessionstore/DocShellCapabilities.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SessionHistory",
  "resource:///modules/sessionstore/SessionHistory.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SessionStorage",
  "resource:///modules/sessionstore/SessionStorage.jsm");

/**
 * Listens for and handles content events that we need for the
 * session store service to be notified of state changes in content.
 */
let EventListener = {

  DOM_EVENTS: [
    "pageshow", "change", "input", "MozStorageChanged"
  ],

  init: function () {
    this.DOM_EVENTS.forEach(e => addEventListener(e, this, true));
  },

  handleEvent: function (event) {
    switch (event.type) {
      case "pageshow":
        if (event.persisted)
          sendAsyncMessage("SessionStore:pageshow");
        break;
      case "input":
      case "change":
        sendAsyncMessage("SessionStore:input");
        break;
      case "MozStorageChanged": {
        let isSessionStorage = true;
        // We are only interested in sessionStorage events
        try {
          if (event.storageArea != content.sessionStorage) {
            isSessionStorage = false;
          }
        } catch (ex) {
          // This page does not even have sessionStorage
          // (this is typically the case of about: pages)
          isSessionStorage = false;
        }
        if (isSessionStorage) {
          sendAsyncMessage("SessionStore:MozStorageChanged");
        }
        break;
      }
      default:
        debug("received unknown event '" + event.type + "'");
        break;
    }
  }
};

/**
 * Listens for and handles messages sent by the session store service.
 */
let MessageListener = {

  MESSAGES: [
    "SessionStore:collectSessionHistory",
    "SessionStore:collectSessionStorage",
    "SessionStore:collectDocShellCapabilities"
  ],

  init: function () {
    this.MESSAGES.forEach(m => addMessageListener(m, this));
  },

  receiveMessage: function ({name, data: {id}}) {
    switch (name) {
      case "SessionStore:collectSessionHistory":
        let history = SessionHistory.read(docShell);
        sendAsyncMessage(name, {id: id, data: history});
        break;
      case "SessionStore:collectSessionStorage":
        let storage = SessionStorage.serialize(docShell);
        sendAsyncMessage(name, {id: id, data: storage});
        break;
      case "SessionStore:collectDocShellCapabilities":
        let disallow = DocShellCapabilities.collect(docShell);
        sendAsyncMessage(name, {id: id, data: disallow});
        break;
      default:
        debug("received unknown message '" + name + "'");
        break;
    }
  }
};

let ProgressListener = {
  init: function() {
    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_LOCATION);
  },
  onLocationChange: function(aWebProgress, aRequest, aLocation, aFlags) {
    // We are changing page, so time to invalidate the state of the tab
    sendAsyncMessage("SessionStore:loadStart");
  },
  onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus) {},
  onProgressChange: function() {},
  onStatusChange: function() {},
  onSecurityChange: function() {},
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference])
};

EventListener.init();
MessageListener.init();
ProgressListener.init();
