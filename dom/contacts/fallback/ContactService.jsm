/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DEBUG = false;
function debug(s) { dump("-*- Fallback ContactService component: " + s + "\n"); }

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

this.EXPORTED_SYMBOLS = [];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/ContactDB.jsm");
Cu.import("resource://gre/modules/PhoneNumberUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageListenerManager");

let myGlobal = this;

let ContactService = {
  init: function() {
    if (DEBUG) debug("Init");
    this._messages = ["Contacts:Find", "Contacts:GetAll", "Contacts:GetAll:SendNow",
                      "Contacts:Clear", "Contact:Save",
                      "Contact:Remove", "Contacts:RegisterForMessages",
                      "child-process-shutdown", "Contacts:GetRevision",
                      "Contacts:GetCount"];
    this._children = [];
    this._cursors = {};
    this._messages.forEach(function(msgName) {
      ppmm.addMessageListener(msgName, this);
    }.bind(this));

    var idbManager = Components.classes["@mozilla.org/dom/indexeddb/manager;1"].getService(Ci.nsIIndexedDatabaseManager);
    idbManager.initWindowless(myGlobal);
    this._db = new ContactDB(myGlobal);
    this._db.init(myGlobal);

    let countryName = PhoneNumberUtils.getCountryName();
    if (Services.prefs.getPrefType("dom.phonenumber.substringmatching." + countryName) == Ci.nsIPrefBranch.PREF_INT) {
      if (DEBUG) debug("Enable Substring Matching for Phone Numbers: " + countryName);
      let val = Services.prefs.getIntPref("dom.phonenumber.substringmatching." + countryName);
      if (val && val > 0) {
        this._db.enableSubstringMatching(val);
      }
    }

    Services.obs.addObserver(this, "profile-before-change", false);
    Services.prefs.addObserver("dom.phonenumber.substringmatching", this, false);
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic === 'profile-before-change') {
      myGlobal = null;
      this._messages.forEach(function(msgName) {
        ppmm.removeMessageListener(msgName, this);
      }.bind(this));
      Services.obs.removeObserver(this, "profile-before-change");
      Services.prefs.removeObserver("dom.phonenumber.substringmatching", this);
      ppmm = null;
      this._messages = null;
      if (this._db)
        this._db.close();
      this._db = null;
      this._children = null;
      this._cursors = null;
    } else if (aTopic === 'nsPref:changed' && aData.contains("dom.phonenumber.substringmatching")) {
      // We don't fully support changing substringMatching during runtime. This is mostly for testing.
      let countryName = PhoneNumberUtils.getCountryName();
      if (Services.prefs.getPrefType("dom.phonenumber.substringmatching." + countryName) == Ci.nsIPrefBranch.PREF_INT) {
        let val = Services.prefs.getIntPref("dom.phonenumber.substringmatching." + countryName);
        if (val && val > 0) {
          this._db.enableSubstringMatching(val);
        }
      }
    }
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
        if (!this._cursors[mm]) {
          this._cursors[mm] = [];
        }
        this._cursors[mm].push(msg.cursorId);

        this._db.getAll(
          function(aContacts) {
            try {
              mm.sendAsyncMessage("Contacts:GetAll:Next", {cursorId: msg.cursorId, contacts: aContacts});
              if (aContacts === null) {
                let index = this._cursors[mm].indexOf(msg.cursorId);
                this._cursors[mm].splice(index, 1);
              }
            } catch (e) {
              if (DEBUG) debug("Child is dead, DB should stop sending contacts");
              throw e;
            }
          }.bind(this),
          function(aErrorMsg) { mm.sendAsyncMessage("Contacts:Find:Return:KO", { requestID: msg.cursorId, errorMsg: aErrorMsg }); },
          msg.findOptions, msg.cursorId);
        break;
      case "Contacts:GetAll:SendNow":
        // sendNow is a no op if there isn't an existing cursor in the DB, so we
        // don't need to assert the permission again.
        this._db.sendNow(msg.cursorId);
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
      case "Contacts:GetRevision":
        if (!this.assertPermission(aMessage, "contacts-read")) {
          return null;
        }
        this._db.getRevision(
          function(revision) {
            mm.sendAsyncMessage("Contacts:Revision", {
              requestID: msg.requestID,
              revision: revision
            });
          }
        );
        break;
      case "Contacts:GetCount":
        if (!this.assertPermission(aMessage, "contacts-read")) {
          return null;
        }
        this._db.getCount(
          function(count) {
            mm.sendAsyncMessage("Contacts:Count", {
              requestID: msg.requestID,
              count: count
            });
          }
        );
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
        let index = this._children.indexOf(mm);
        if (index != -1) {
          if (DEBUG) debug("Unregister index: " + index);
          this._children.splice(index, 1);
        }
        if (this._cursors[mm]) {
          for (let id of this._cursors[mm]) {
            this._db.clearDispatcher(id);
          }
          delete this._cursors[mm];
        }
        break;
      default:
        if (DEBUG) debug("WRONG MESSAGE NAME: " + aMessage.name);
    }
  }
}

ContactService.init();
