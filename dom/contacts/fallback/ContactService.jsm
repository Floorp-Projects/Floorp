/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DEBUG = false;
function debug(s) { dump("-*- Fallback ContactService component: " + s + "\n"); }

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

this.EXPORTED_SYMBOLS = ["DOMContactManager"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/ContactDB.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageListenerManager");

XPCOMUtils.defineLazyGetter(this, "mRIL", function () {
  let telephony = Cc["@mozilla.org/ril;1"];
  if (!telephony) {
    // Return a mock RIL because B2G Desktop build does not support telephony.
    return {
      getICCContacts: function(aContactType, aCallback) {
        aCallback("!telephony", null, null);
      }
    };
  }
  return telephony.getService(Ci.nsIRadioInterfaceLayer);
});

let myGlobal = this;

this.DOMContactManager = {
  // maps children to their live cursors so we can cleanup on shutdown/crash
  _liveCursors: {},

  init: function() {
    if (DEBUG) debug("Init");
    this._messages = ["Contacts:Find", "Contacts:GetAll", "Contacts:GetAll:Continue", "Contacts:Clear", "Contact:Save",
                      "Contact:Remove", "Contacts:GetSimContacts",
                      "Contacts:RegisterForMessages", "child-process-shutdown"];
    this._children = [];
    this._messages.forEach(function(msgName) {
      ppmm.addMessageListener(msgName, this);
    }.bind(this));

    var idbManager = Components.classes["@mozilla.org/dom/indexeddb/manager;1"].getService(Ci.nsIIndexedDatabaseManager);
    idbManager.initWindowless(myGlobal);
    this._db = new ContactDB(myGlobal);
    this._db.init(myGlobal);

    Services.obs.addObserver(this, "profile-before-change", false);
  },

  observe: function(aSubject, aTopic, aData) {
    myGlobal = null;
    this._messages.forEach(function(msgName) {
      ppmm.removeMessageListener(msgName, this);
    }.bind(this));
    Services.obs.removeObserver(this, "profile-before-change");
    ppmm = null;
    this._messages = null;
    if (this._db)
      this._db.close();
    this._db = null;
  },

  assertPermission: function(aMessage, aPerm) {
    if (!aMessage.target.assertPermission(aPerm)) {
      Cu.reportError("Contacts message " + aMessage.name +
                     " from a content process with no" + aPerm + " privileges.");
      return false;
    }
    return true;
  },

  broadcastMessage: function broadcastMessage(aMsgName, aContent) {
    this._children.forEach(function(msgMgr) {
      msgMgr.sendAsyncMessage(aMsgName, aContent);
    });
  },

  receiveMessage: function(aMessage) {
    if (DEBUG) debug("receiveMessage " + aMessage.name);
    let mm = aMessage.target;
    let msg = aMessage.data;

    switch (aMessage.name) {
      case "Contacts:Find":
        if (!this.assertPermission(aMessage, "contacts-read")) {
          return null;
        }
        let result = [];
        this._db.find(
          function(contacts) {
            for (let i in contacts) {
              result.push(contacts[i]);
            }

            if (DEBUG) debug("result:" + JSON.stringify(result));
            mm.sendAsyncMessage("Contacts:Find:Return:OK", {requestID: msg.requestID, contacts: result});
          }.bind(this),
          function(aErrorMsg) { mm.sendAsyncMessage("Contacts:Find:Return:KO", { requestID: msg.requestID, errorMsg: aErrorMsg }); }.bind(this),
          msg.options.findOptions);
        break;
      case "Contacts:GetAll":
        if (!this.assertPermission(aMessage, "contacts-read")) {
          return null;
        }
        this._db.getAll(
          function(aContact) {
            mm.sendAsyncMessage("Contacts:GetAll:Next", {cursorId: msg.cursorId, contact: aContact});
          },
          function(aErrorMsg) { mm.sendAsyncMessage("Contacts:Find:Return:KO", { errorMsg: aErrorMsg }); },
          msg.findOptions, msg.cursorId);
        if (Array.isArray(this._liveCursors[mm])) {
          this._liveCursors[mm].push(msg.cursorId);
        } else {
          this._liveCursors[mm] = [msg.cursorId];
        }
        break;
      case "Contacts:GetAll:Continue":
        this._db.getNext(
          function(aContact) {
            if (aContact == null) { // last contact, release the cursor
              let cursors = this._liveCursors[mm];
              cursors.splice(cursors.indexOf(msg.cursorId), 1);
            }
            mm.sendAsyncMessage("Contacts:GetAll:Next", {cursorId: msg.cursorId, contact: aContact});
          }.bind(this),
          function(aErrorMsg) { mm.sendAsyncMessage("Contacts:Find:Return:KO", { errorMsg: aErrorMsg }); },
          msg.cursorId);
        break;
      case "Contact:Save":
        if (msg.options.reason === "create") {
          if (!this.assertPermission(aMessage, "contacts-create")) {
            return null;
          }
        } else {
          if (!this.assertPermission(aMessage, "contacts-write")) {
            return null;
          }
        }
        this._db.saveContact(
          msg.options.contact,
          function() {
            mm.sendAsyncMessage("Contact:Save:Return:OK", { requestID: msg.requestID, contactID: msg.options.contact.id });
            this.broadcastMessage("Contact:Changed", { contactID: msg.options.contact.id, reason: msg.options.reason });
          }.bind(this),
          function(aErrorMsg) { mm.sendAsyncMessage("Contact:Save:Return:KO", { requestID: msg.requestID, errorMsg: aErrorMsg }); }.bind(this)
        );
        break;
      case "Contact:Remove":
        if (!this.assertPermission(aMessage, "contacts-write")) {
          return null;
        }
        this._db.removeContact(
          msg.options.id,
          function() {
            mm.sendAsyncMessage("Contact:Remove:Return:OK", { requestID: msg.requestID, contactID: msg.options.id });
            this.broadcastMessage("Contact:Changed", { contactID: msg.options.id, reason: "remove" });
          }.bind(this),
          function(aErrorMsg) { mm.sendAsyncMessage("Contact:Remove:Return:KO", { requestID: msg.requestID, errorMsg: aErrorMsg }); }.bind(this)
        );
        break;
      case "Contacts:Clear":
        if (!this.assertPermission(aMessage, "contacts-write")) {
          return null;
        }
        this._db.clear(
          function() {
            mm.sendAsyncMessage("Contacts:Clear:Return:OK", { requestID: msg.requestID });
            this.broadcastMessage("Contact:Changed", { reason: "remove" });
          }.bind(this),
          function(aErrorMsg) { mm.sendAsyncMessage("Contacts:Clear:Return:KO", { requestID: msg.requestID, errorMsg: aErrorMsg }); }.bind(this)
        );
        break;
      case "Contacts:GetSimContacts":
        if (!this.assertPermission(aMessage, "contacts-read")) {
          return null;
        }
        mRIL.getICCContacts(
          msg.options.contactType,
          function (aErrorMsg, aType, aContacts) {
            if (aErrorMsg !== 'undefined') {
              mm.sendAsyncMessage("Contacts:GetSimContacts:Return:KO",
                                  {requestID: msg.requestID,
                                   errorMsg: aErrorMsg});
            } else {
              mm.sendAsyncMessage("Contacts:GetSimContacts:Return:OK",
                                  {requestID: msg.requestID,
                                   contacts: aContacts});
            }
          }.bind(this));
        break;
      case "Contacts:RegisterForMessages":
        if (!aMessage.target.assertPermission("contacts-read")) {
          return null;
        }
        if (DEBUG) debug("Register!");
        if (this._children.indexOf(mm) == -1) {
          this._children.push(mm);
        }
        break;
      case "child-process-shutdown":
        if (DEBUG) debug("Unregister");
        this._db.releaseCursors(this._liveCursors[mm]);
        delete this._liveCursors[mm];
        let index = this._children.indexOf(mm);
        if (index != -1) {
          if (DEBUG) debug("Unregister index: " + index);
          this._children.splice(index, 1);
        }
        break;
      default:
        if (DEBUG) debug("WRONG MESSAGE NAME: " + aMessage.name);
    }
  }
}

DOMContactManager.init();
