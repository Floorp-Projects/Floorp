/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "cpmm", function() {
  return Cc["@mozilla.org/childprocessmessagemanager;1"]
         .getService(Ci.nsIFrameMessageManager)
         .QueryInterface(Ci.nsISyncMessageSender);
});

// Limit the number of pending messages for a given type.
let kMaxPendingMessages;
try {
  kMaxPendingMessages = Services.prefs.getIntPref("dom.messages.maxPendingMessages");
} catch(e) {
  // getIntPref throws when the pref is not set.
  kMaxPendingMessages = 5;
}

function debug(aMsg) { 
  //dump("-- SystemMessageManager " + Date.now() + " : " + aMsg + "\n"); 
}

// Implementation of the DOM API for system messages

function SystemMessageManager() {
  // Message handlers for this page.
  // We can have only one handler per message type.
  this._handlers = {};

  // Pending messages for this page, keyed by message type.
  this._pendings = {};
}

SystemMessageManager.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  mozSetMessageHandler: function sysMessMgr_setMessageHandler(aType, aHandler) {
    debug("setMessage handler for [" + aType + "] " + aHandler);
    if (!aType) {
      // Just bail out if we have no type.
      return;
    }

    let handlers = this._handlers;
    if (!aHandler) {
      // Setting the handler to null means we don't want to receive messages
      // of this type anymore.
      delete handlers[aType];
      return;
    }

    // Last registered handler wins.
    handlers[aType] = aHandler;

    // If we have pending messages, send them asynchronously.
    if (this._getPendingMessages(aType, true)) {
      let thread = Services.tm.mainThread;
      let pending = this._pendings[aType];
      this._pendings[aType] = [];
      pending.forEach(function dispatch_pending(aPending) {
        thread.dispatch({
          run: function run() {
            aHandler.handleMessage(aPending);
          }
        }, Ci.nsIEventTarget.DISPATCH_NORMAL);
      });
    }
  },

  _getPendingMessages: function sysMessMgr_getPendingMessages(aType, aForceUpdate) {
    debug("hasPendingMessage " + aType);
    let pendings = this._pendings;

    // If we have a handler for this type, we can't have any pending message.
    // If called from setMessageHandler, we still want to update the pending
    // queue to deliver existing messages.
    if (aType in this._handlers && !aForceUpdate) {
      return false;
    }

    // Send a sync message to the parent to check if we have a pending message 
    // for this type.
    let messages = cpmm.sendSyncMessage("SystemMessageManager:GetPending", 
                                        { type: aType,
                                          uri: this._uri,
                                          manifest: this._manifest })[0];
    if (!messages) {
      // No new pending messages, but the queue may not be empty yet.
      return pendings[aType] && pendings[aType].length != 0;
    }

    if (!pendings[aType]) {
      pendings[aType] = [];
    }

    // Doing that instead of pending.concat() to avoid array copy.
    messages.forEach(function hpm_addPendings(aMessage) {
      pendings[aType].push(aMessage);
      if (pendings[aType].length > kMaxPendingMessages) {
        pendings[aType].splice(0, 1);
      }
    });

    return pendings[aType].length != 0;
  },

  mozHasPendingMessage: function sysMessMgr_hasPendingMessage(aType) {
    return this._getPendingMessages(aType, false);
  },

  uninit: function sysMessMgr_uninit()  {
    this._handlers = null;
    this._pendings =  null;
  },

  receiveMessage: function sysMessMgr_receiveMessage(aMessage) {
    debug("receiveMessage " + aMessage.name + " - " + 
          aMessage.json.type + " for " + aMessage.json.manifest +
          " (" + this._manifest + ")");

    let msg = aMessage.json;
    if (msg.manifest != this._manifest)
      return;

    // Bail out if we have no handlers registered for this type.
    if (!(msg.type in this._handlers)) {
      debug("No handler for this type");
      return;
    }

    this._handlers[msg.type].handleMessage(msg.msg);
  },

  // nsIDOMGlobalPropertyInitializer implementation.
  init: function sysMessMgr_init(aWindow) {
    debug("init");
    this.initHelper(aWindow, ["SystemMessageManager:Message"]);
    this._uri = aWindow.document.nodePrincipal.URI.spec;
    let utils = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Components.interfaces.nsIDOMWindowUtils);

    // Get the manifest url is this is an installed app.
    this._manifest = null;
    let app = utils.getApp();
    if (app)
      this._manifest = app.manifestURL;
  },

  classID: Components.ID("{bc076ea0-609b-4d8f-83d7-5af7cbdc3bb2}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMNavigatorSystemMessages,
                                         Ci.nsIDOMGlobalPropertyInitializer]),

  classInfo: XPCOMUtils.generateCI({classID: Components.ID("{bc076ea0-609b-4d8f-83d7-5af7cbdc3bb2}"),
                                    contractID: "@mozilla.org/system-message-manager;1",
                                    interfaces: [Ci.nsIDOMNavigatorSystemMessages],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT,
                                    classDescription: "System Messages"})
}

const NSGetFactory = XPCOMUtils.generateNSGetFactory([SystemMessageManager]);
