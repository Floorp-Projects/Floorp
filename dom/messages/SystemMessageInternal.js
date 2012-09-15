/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");

// Limit the number of pending messages for a given page.
let kMaxPendingMessages;
try {
  kMaxPendingMessages = Services.prefs.getIntPref("dom.messages.maxPendingMessages");
} catch(e) {
  // getIntPref throws when the pref is not set.
  kMaxPendingMessages = 5;
}

function debug(aMsg) { 
  //dump("-- SystemMessageInternal " + Date.now() + " : " + aMsg + "\n");
}

// Implementation of the component used by internal users.

function SystemMessageInternal() {
  // The set of pages registered by installed apps. We keep the
  // list of pending messages for each page here also.
  this._pages = [];
  Services.obs.addObserver(this, "xpcom-shutdown", false);
  ppmm.addMessageListener("SystemMessageManager:GetPending", this);
}

SystemMessageInternal.prototype = {
  sendMessage: function sendMessage(aType, aMessage, aPageURI, aManifestURI) {
    debug("Broadcasting " + aType + " " + JSON.stringify(aMessage));
    ppmm.broadcastAsyncMessage("SystemMessageManager:Message" , { type: aType,
                                                                  msg: aMessage,
                                                                  manifest: aManifestURI.spec });
    this._pages.forEach(function sendMess_openPage(aPage) {
      if (aPage.type != aType ||
          aPage.manifest != aManifestURI.spec ||
          aPage.uri != aPageURI.spec) {
        return;
      }

      this._processPage(aPage, aMessage);
    }.bind(this))
  },

  broadcastMessage: function broadcastMessage(aType, aMessage) {
    debug("Broadcasting " + aType + " " + JSON.stringify(aMessage));
    // Find pages that registered an handler for this type.
    this._pages.forEach(function(aPage) {
      if (aPage.type == aType) {
        ppmm.broadcastAsyncMessage("SystemMessageManager:Message" , { type: aType,
                                                                      msg: aMessage,
                                                                      manifest: aPage.manifest });

        this._processPage(aPage, aMessage);
      }
    }.bind(this))
  },

  registerPage: function registerPage(aType, aPageURI, aManifestURI) {
    if (!aPageURI || !aManifestURI) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    this._pages.push({ type: aType,
                       uri: aPageURI.spec,
                       manifest: aManifestURI.spec,
                       pending: [] });
  },

  receiveMessage: function receiveMessage(aMessage) {
    debug("received SystemMessageManager:GetPending " + aMessage.json.type + " for " + aMessage.json.uri + " @ " + aMessage.json.manifest);
    // This is a sync call, use to return the pending message for a page.
    let msg = aMessage.json;
    debug(JSON.stringify(msg));

    // Find the right page.
    let page = null;
    this._pages.some(function(aPage) {
      if (aPage.uri == msg.uri &&
          aPage.type == msg.type &&
          aPage.manifest == msg.manifest) {
        page = aPage;
      }
      return page !== null;
    });

    if (!page) {
      return null;
    }

    let pending = page.pending;
    // Clear the pending queue for this page.
    // This is ok since we'll store pending events in SystemMessageManager.js
    page.pending = [];

    return pending;
  },

  observe: function observe(aSubject, aTopic, aData) {
    if (aTopic == "xpcom-shutdown") {
      ppmm.removeMessageListener("SystemMessageManager:GetPending", this);
      Services.obs.removeObserver(this, "xpcom-shutdown");
      ppmm = null;
      this._pages = null;
    }
  },

  _processPage: function _processPage(aPage, aMessage) {
    // Queue the message for the page.
    aPage.pending.push(aMessage);
    if (aPage.pending.length > kMaxPendingMessages) {
      aPage.pending.splice(0, 1);
    }

    // We don't need to send the full object to observers.
    let page = { uri: aPage.uri,
                 manifest: aPage.manifest,
                 type: aPage.type,
                 target: aMessage.target };
    debug("Asking to open  " + JSON.stringify(page));
    Services.obs.notifyObservers(this, "system-messages-open-app", JSON.stringify(page));
  },

  classID: Components.ID("{70589ca5-91ac-4b9e-b839-d6a88167d714}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsISystemMessagesInternal, Ci.nsIObserver])
}

const NSGetFactory = XPCOMUtils.generateNSGetFactory([SystemMessageInternal]);
