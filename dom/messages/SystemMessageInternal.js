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
Cu.import("resource://gre/modules/SystemMessagePermissionsChecker.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");

XPCOMUtils.defineLazyServiceGetter(this, "gUUIDGenerator",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

// Limit the number of pending messages for a given page.
let kMaxPendingMessages;
try {
  kMaxPendingMessages = Services.prefs.getIntPref("dom.messages.maxPendingMessages");
} catch(e) {
  // getIntPref throws when the pref is not set.
  kMaxPendingMessages = 5;
}

const kMessages =["SystemMessageManager:GetPendingMessages",
                  "SystemMessageManager:HasPendingMessages",
                  "SystemMessageManager:Register",
                  "SystemMessageManager:Unregister",
                  "SystemMessageManager:Message:Return:OK",
                  "SystemMessageManager:AskReadyToRegister",
                  "child-process-shutdown"]

function debug(aMsg) {
  //dump("-- SystemMessageInternal " + Date.now() + " : " + aMsg + "\n");
}

// Implementation of the component used by internal users.

function SystemMessageInternal() {
  // The set of pages registered by installed apps. We keep the
  // list of pending messages for each page here also.
  this._pages = [];
  this._listeners = {};

  this._webappsRegistryReady = false;
  this._bufferedSysMsgs = [];

  Services.obs.addObserver(this, "xpcom-shutdown", false);
  Services.obs.addObserver(this, "webapps-registry-start", false);
  Services.obs.addObserver(this, "webapps-registry-ready", false);
  kMessages.forEach(function(aMsg) {
    ppmm.addMessageListener(aMsg, this);
  }, this);

  Services.obs.notifyObservers(this, "system-message-internal-ready", null);
}

SystemMessageInternal.prototype = {
  sendMessage: function sendMessage(aType, aMessage, aPageURI, aManifestURI) {
    // Buffer system messages until the webapps' registration is ready,
    // so that we can know the correct pages registered to be sent.
    if (!this._webappsRegistryReady) {
      this._bufferedSysMsgs.push({ how: "send",
                                   type: aType,
                                   msg: aMessage,
                                   pageURI: aPageURI,
                                   manifestURI: aManifestURI });
      return;
    }

    // Give this message an ID so that we can identify the message and
    // clean it up from the pending message queue when apps receive it.
    let messageID = gUUIDGenerator.generateUUID().toString();

    debug("Sending " + aType + " " + JSON.stringify(aMessage) +
      " for " + aPageURI.spec + " @ " + aManifestURI.spec);

    // Don't need to open the pages and queue the system message
    // which was not allowed to be sent.
    if (!this._sendMessageCommon(aType,
                                 aMessage,
                                 messageID,
                                 aPageURI.spec,
                                 aManifestURI.spec)) {
      return;
    }

    let pagesToOpen = {};
    this._pages.forEach(function(aPage) {
      if (!this._isPageMatched(aPage, aType, aPageURI.spec, aManifestURI.spec)) {
        return;
      }

      // Queue this message in the corresponding pages.
      this._queueMessage(aPage, aMessage, messageID);

      // Open app pages to handle their pending messages.
      // Note that we only need to open each app page once.
      let key = this._createKeyForPage(aPage);
      if (!pagesToOpen.hasOwnProperty(key)) {
        this._openAppPage(aPage, aMessage);
        pagesToOpen[key] = true;
      }
    }, this);
  },

  broadcastMessage: function broadcastMessage(aType, aMessage) {
    // Buffer system messages until the webapps' registration is ready,
    // so that we can know the correct pages registered to be broadcasted.
    if (!this._webappsRegistryReady) {
      this._bufferedSysMsgs.push({ how: "broadcast",
                                   type: aType,
                                   msg: aMessage });
      return;
    }

    // Give this message an ID so that we can identify the message and
    // clean it up from the pending message queue when apps receive it.
    let messageID = gUUIDGenerator.generateUUID().toString();

    debug("Broadcasting " + aType + " " + JSON.stringify(aMessage));
    // Find pages that registered an handler for this type.
    let pagesToOpen = {};
    this._pages.forEach(function(aPage) {
      if (aPage.type == aType) {
        // Don't need to open the pages and queue the system message
        // which was not allowed to be sent.
        if (!this._sendMessageCommon(aType,
                                     aMessage,
                                     messageID,
                                     aPage.uri,
                                     aPage.manifest)) {
          return;
        }

        // Queue this message in the corresponding pages.
        this._queueMessage(aPage, aMessage, messageID);

        // Open app pages to handle their pending messages.
        // Note that we only need to open each app page once.
        let key = this._createKeyForPage(aPage);
        if (!pagesToOpen.hasOwnProperty(key)) {
          this._openAppPage(aPage, aMessage);
          pagesToOpen[key] = true;
        }
      }
    }, this);
  },

  registerPage: function registerPage(aType, aPageURI, aManifestURI) {
    if (!aPageURI || !aManifestURI) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    this._pages.push({ type: aType,
                       uri: aPageURI.spec,
                       manifest: aManifestURI.spec,
                       pendingMessages: [] });
  },

  receiveMessage: function receiveMessage(aMessage) {
    let msg = aMessage.json;

    // To prevent the hacked child process from sending commands to parent
    // to manage system messages, we need to check its manifest URL.
    if (["SystemMessageManager:Register",
         "SystemMessageManager:Unregister",
         "SystemMessageManager:GetPendingMessages",
         "SystemMessageManager:HasPendingMessages",
         "SystemMessageManager:Message:Return:OK"].indexOf(aMessage.name) != -1) {
      if (!aMessage.target.assertContainApp(msg.manifest)) {
        debug("Got message from a child process containing illegal manifest URL.");
        return null;
      }
    }

    switch(aMessage.name) {
      case "SystemMessageManager:AskReadyToRegister":
        return true;
        break;
      case "SystemMessageManager:Register":
      {
        debug("Got Register from " + msg.manifest);
        if (!this._listeners[msg.manifest]) {
          this._listeners[msg.manifest] = {};
        }
        this._listeners[msg.manifest][msg.innerWindowID] = aMessage.target;
        debug("listeners for " + msg.manifest + " innerWinID " + msg.innerWindowID);
        break;
      }
      case "child-process-shutdown":
      {
        debug("Got child-process-shutdown from " + aMessage.target);
        for (let manifest in this._listeners) {
          for (let winID in this._listeners[manifest]) {
            if (aMessage.target === this._listeners[manifest][winID]) {
              debug("remove " + manifest );
              delete this._listeners[manifest];
              return;
            }
          }
        }
        break;
      }
      case "SystemMessageManager:Unregister":
      {
        debug("Got Unregister from " + aMessage.target + "innerWinID " + msg.innerWindowID);
        delete this._listeners[msg.manifest][msg.innerWindowID];
        debug("Removing " + aMessage.target + "innerWinID " + msg.innerWindowID );

        break;
      }
      case "SystemMessageManager:GetPendingMessages":
      {
        debug("received SystemMessageManager:GetPendingMessages " + msg.type +
          " for " + msg.uri + " @ " + msg.manifest);

        // This is a sync call used to return the pending messages for a page.
        // Find the right page to get its corresponding pending messages.
        let page = null;
        this._pages.some(function(aPage) {
          if (this._isPageMatched(aPage, msg.type, msg.uri, msg.manifest)) {
            page = aPage;
          }
          return page !== null;
        }, this);
        if (!page) {
          return;
        }

        // Return the |msg| of each pending message (drop the |msgID|).
        let pendingMessages = [];
        page.pendingMessages.forEach(function(aMessage) {
          pendingMessages.push(aMessage.msg);
        });

        // Clear the pending queue for this page. This is OK since we'll store
        // pending messages in the content process (|SystemMessageManager|).
        page.pendingMessages.length = 0;

        // Send the array of pending messages.
        aMessage.target.sendAsyncMessage("SystemMessageManager:GetPendingMessages:Return",
                                         { type: msg.type,
                                           manifest: msg.manifest,
                                           uri: msg.uri,
                                           msgQueue: pendingMessages });
        break;
      }
      case "SystemMessageManager:HasPendingMessages":
      {
        debug("received SystemMessageManager:HasPendingMessages " + msg.type +
          " for " + msg.uri + " @ " + msg.manifest);

        // This is a sync call used to return if a page has pending messages.
        // Find the right page to get its corresponding pending messages.
        let page = null;
        this._pages.some(function(aPage) {
          if (this._isPageMatched(aPage, msg.type, msg.uri, msg.manifest)) {
            page = aPage;
          }
          return page !== null;
        }, this);
        if (!page) {
          return false;
        }

        return page.pendingMessages.length != 0;
        break;
      }
      case "SystemMessageManager:Message:Return:OK":
      {
        debug("received SystemMessageManager:Message:Return:OK " + msg.type +
          " for " + msg.uri + " @ " + msg.manifest);

        // We need to clean up the pending message since the app has already
        // received it, thus avoiding the re-lanunched app handling it again.
        this._pages.forEach(function(aPage) {
          if (!this._isPageMatched(aPage, msg.type, msg.uri, msg.manifest)) {
            return;
          }

          let pendingMessages = aPage.pendingMessages;
          for (let i = 0; i < pendingMessages.length; i++) {
            if (pendingMessages[i].msgID === msg.msgID) {
              pendingMessages.splice(i, 1);
              break;
            }
          }
        }, this);
        break;
      }
    }
  },

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "xpcom-shutdown":
        kMessages.forEach(function(aMsg) {
          ppmm.removeMessageListener(aMsg, this);
        }, this);
        Services.obs.removeObserver(this, "xpcom-shutdown");
        Services.obs.removeObserver(this, "webapps-registry-start");
        Services.obs.removeObserver(this, "webapps-registry-ready");
        ppmm = null;
        this._pages = null;
        this._bufferedSysMsgs = null;
        break;
      case "webapps-registry-start":
        this._webappsRegistryReady = false;
        break;
      case "webapps-registry-ready":
        // After the webapps' registration has been done for sure,
        // re-fire the buffered system messages if there is any.
        this._webappsRegistryReady = true;
        this._bufferedSysMsgs.forEach(function(aSysMsg) {
          switch (aSysMsg.how) {
            case "send":
              this.sendMessage(
                aSysMsg.type, aSysMsg.msg, aSysMsg.pageURI, aSysMsg.manifestURI);
              break;
            case "broadcast":
              this.broadcastMessage(aSysMsg.type, aSysMsg.msg);
              break;
          }
        }, this);
        this._bufferedSysMsgs.length = 0;
        break;
    }
  },

  _queueMessage: function _queueMessage(aPage, aMessage, aMessageID) {
    // Queue the message for this page because we've never known if an app is
    // opened or not. We'll clean it up when the app has already received it.
    aPage.pendingMessages.push({ msg: aMessage, msgID: aMessageID });
    if (aPage.pendingMessages.length > kMaxPendingMessages) {
      aPage.pendingMessages.splice(0, 1);
    }
  },

  _openAppPage: function _openAppPage(aPage, aMessage) {
    // We don't need to send the full object to observers.
    let page = { uri: aPage.uri,
                 manifest: aPage.manifest,
                 type: aPage.type,
                 target: aMessage.target };
    debug("Asking to open " + JSON.stringify(page));
    Services.obs.notifyObservers(this, "system-messages-open-app", JSON.stringify(page));
  },

  _isPageMatched: function _isPageMatched(aPage, aType, aPageURI, aManifestURI) {
    return (aPage.type === aType &&
            aPage.manifest === aManifestURI &&
            aPage.uri === aPageURI)
  },

  _createKeyForPage: function _createKeyForPage(aPage) {
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                      .createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";

    let hasher = Cc["@mozilla.org/security/hash;1"]
                   .createInstance(Ci.nsICryptoHash);
    hasher.init(hasher.SHA1);

    // add uri and action to the hash
    ["type", "manifest", "uri"].forEach(function(aProp) {
      let data = converter.convertToByteArray(aPage[aProp], {});
      hasher.update(data, data.length);
    });

    return hasher.finish(true);
  },

  _sendMessageCommon:
    function _sendMessageCommon(aType, aMessage, aMessageID, aPageURI, aManifestURI) {
    // Don't send the system message not granted by the app's permissions.
    if (!SystemMessagePermissionsChecker
          .isSystemMessagePermittedToSend(aType,
                                          aPageURI,
                                          aManifestURI)) {
      return false;
    }

    let winTargets = this._listeners[aManifestURI];
    if (winTargets) {
      for (let winID in winTargets) {
        winTargets[winID].sendAsyncMessage("SystemMessageManager:Message",
                                           { type: aType,
                                             msg: aMessage,
                                             manifest: aManifestURI,
                                             uri: aPageURI,
                                             msgID: aMessageID });
      }
    }

    return true;
  },

  classID: Components.ID("{70589ca5-91ac-4b9e-b839-d6a88167d714}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsISystemMessagesInternal, Ci.nsIObserver])
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SystemMessageInternal]);
