/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DEBUG = false;
function debug(s) { dump("-*- ContactManager: " + s + "\n"); }

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

XPCOMUtils.defineLazyServiceGetter(Services, "DOMRequest",
                                   "@mozilla.org/dom/dom-request-service;1",
                                   "nsIDOMRequestService");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

const CONTACTS_SENDMORE_MINIMUM = 5;

// We need this to create a copy of the mozContact object in ContactManager.save
// Keep in sync with the interfaces.
const PROPERTIES = [
  "name", "honorificPrefix", "givenName", "additionalName", "familyName",
  "phoneticGivenName", "phoneticFamilyName",
  "honorificSuffix", "nickname", "photo", "category", "org", "jobTitle",
  "bday", "note", "anniversary", "sex", "genderIdentity", "key", "adr", "email",
  "url", "impp", "tel"
];

let mozContactInitWarned = false;

function Contact() { }

Contact.prototype = {
  __init: function(aProp) {
    for (let prop in aProp) {
      this[prop] = aProp[prop];
    }
  },

  init: function(aProp) {
    // init is deprecated, warn once in the console if it's used
    if (!mozContactInitWarned) {
      mozContactInitWarned = true;
      Cu.reportError("mozContact.init is DEPRECATED. Use the mozContact constructor instead. " +
                     "See https://developer.mozilla.org/docs/WebAPI/Contacts for details.");
    }

    for (let prop of PROPERTIES) {
      this[prop] = aProp[prop];
    }
  },

  setMetadata: function(aId, aPublished, aUpdated) {
    this.id = aId;
    if (aPublished) {
      this.published = aPublished;
    }
    if (aUpdated) {
      this.updated = aUpdated;
    }
  },

  classID: Components.ID("{72a5ee28-81d8-4af8-90b3-ae935396cc66}"),
  contractID: "@mozilla.org/contact;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports]),
};

function ContactManager() { }

ContactManager.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,
  hasListenPermission: false,
  _cachedContacts: [] ,

  set oncontactchange(aHandler) {
    this.__DOM_IMPL__.setEventHandler("oncontactchange", aHandler);
  },

  get oncontactchange() {
    return this.__DOM_IMPL__.getEventHandler("oncontactchange");
  },

  _convertContact: function(aContact) {
    let newContact = new this._window.mozContact(aContact.properties);
    newContact.setMetadata(aContact.id, aContact.published, aContact.updated);
    return newContact;
  },

  _convertContacts: function(aContacts) {
    let contacts = new this._window.Array();
    for (let i in aContacts) {
      contacts.push(this._convertContact(aContacts[i]));
    }
    return contacts;
  },

  _fireSuccessOrDone: function(aCursor, aResult) {
    if (aResult == null) {
      Services.DOMRequest.fireDone(aCursor);
    } else {
      Services.DOMRequest.fireSuccess(aCursor, aResult);
    }
  },

  _pushArray: function(aArr1, aArr2) {
    aArr1.push.apply(aArr1, aArr2);
  },

  receiveMessage: function(aMessage) {
    if (DEBUG) debug("receiveMessage: " + aMessage.name);
    let msg = aMessage.json;
    let contacts = msg.contacts;

    let req;
    switch (aMessage.name) {
      case "Contacts:Find:Return:OK":
        req = this.getRequest(msg.requestID);
        if (req) {
          let result = this._convertContacts(contacts);
          Services.DOMRequest.fireSuccess(req.request, result);
        } else {
          if (DEBUG) debug("no request stored!" + msg.requestID);
        }
        break;
      case "Contacts:GetAll:Next":
        let data = this.getRequest(msg.cursorId);
        if (!data) {
          break;
        }
        let result = contacts ? this._convertContacts(contacts) : [null];
        if (data.waitingForNext) {
          if (DEBUG) debug("cursor waiting for contact, sending");
          data.waitingForNext = false;
          let contact = result.shift();
          this._pushArray(data.cachedContacts, result);
          this.nextTick(this._fireSuccessOrDone.bind(this, data.cursor, contact));
          if (!contact) {
            this.removeRequest(msg.cursorId);
          }
        } else {
          if (DEBUG) debug("cursor not waiting, saving");
          this._pushArray(data.cachedContacts, result);
        }
        break;
      case "Contact:Save:Return:OK":
        // If a cached contact was saved and a new contact ID was returned, update the contact's ID
        if (this._cachedContacts[msg.requestID]) {
          if (msg.contactID) {
            this._cachedContacts[msg.requestID].id = msg.contactID;
          }
          delete this._cachedContacts[msg.requestID];
        }
      case "Contacts:Clear:Return:OK":
      case "Contact:Remove:Return:OK":
        req = this.getRequest(msg.requestID);
        if (req)
          Services.DOMRequest.fireSuccess(req.request, null);
        break;
      case "Contacts:Find:Return:KO":
      case "Contact:Save:Return:KO":
      case "Contact:Remove:Return:KO":
      case "Contacts:Clear:Return:KO":
      case "Contacts:GetRevision:Return:KO":
      case "Contacts:Count:Return:KO":
        req = this.getRequest(msg.requestID);
        if (req) {
          if (req.request) {
            req = req.request;
          }
          Services.DOMRequest.fireError(req, msg.errorMsg);
        }
        break;
      case "Contacts:GetAll:Return:KO":
        req = this.getRequest(msg.requestID);
        if (req) {
          Services.DOMRequest.fireError(req.cursor, msg.errorMsg);
        }
        break;
      case "Contact:Changed":
        // Fire oncontactchange event
        if (DEBUG) debug("Contacts:ContactChanged: " + msg.contactID + ", " + msg.reason);
        let event = new this._window.MozContactChangeEvent("contactchange", {
          contactID: msg.contactID,
          reason: msg.reason
        });
        this.dispatchEvent(event);
        break;
      case "Contacts:Revision":
        if (DEBUG) debug("new revision: " + msg.revision);
        req = this.getRequest(msg.requestID);
        if (req) {
          Services.DOMRequest.fireSuccess(req.request, msg.revision);
        }
        break;
      case "Contacts:Count":
        if (DEBUG) debug("count: " + msg.count);
        req = this.getRequest(msg.requestID);
        if (req) {
          Services.DOMRequest.fireSuccess(req.request, msg.count);
        }
        break;
      default:
        if (DEBUG) debug("Wrong message: " + aMessage.name);
    }
    this.removeRequest(msg.requestID);
  },

  dispatchEvent: function(event) {
    if (this.hasListenPermission) {
      this.__DOM_IMPL__.dispatchEvent(event);
    }
  },

  askPermission: function (aAccess, aRequest, aAllowCallback, aCancelCallback) {
    if (DEBUG) debug("askPermission for contacts");

    let access;
    switch(aAccess) {
      case "create":
        access = "create";
        break;
      case "update":
      case "remove":
        access = "write";
        break;
      case "find":
      case "listen":
      case "revision":
      case "count":
        access = "read";
        break;
      default:
        access = "unknown";
      }

    // Shortcut for ALLOW_ACTION so we avoid a parent roundtrip
    let principal = this._window.document.nodePrincipal;
    let type = "contacts-" + access;
    let permValue =
      Services.perms.testExactPermissionFromPrincipal(principal, type);
    if (permValue == Ci.nsIPermissionManager.ALLOW_ACTION) {
      if (aAllowCallback) {
        aAllowCallback();
      }
      return;
    } else if (permValue == Ci.nsIPermissionManager.DENY_ACTION ||
               permValue == Ci.nsIPermissionManager.UNKNOWN_ACTION) {
      if (aCancelCallback) {
        aCancelCallback();
      }
      return;
    }

    // Create an array with a single nsIContentPermissionType element.
    let type = {
      type: "contacts",
      access: access,
      options: [],
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPermissionType])
    };
    let typeArray = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    typeArray.appendElement(type, false);

    // create a nsIContentPermissionRequest
    let request = {
      types: typeArray,
      principal: principal,
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPermissionRequest]),
      allow: aAllowCallback ||
             function() {
               if (DEBUG)
                 debug("Default allow contacts callback. " + access +"\n");
             },
      cancel: aCancelCallback ||
              function() {
                if (DEBUG)
                  debug("Default cancel contacts callback. " + access +"\n");
              },
      window: this._window
    };

    // Using askPermission from nsIDOMWindowUtils that takes care of the
    // remoting if needed.
    let windowUtils = this._window.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDOMWindowUtils);
    windowUtils.askPermission(request);
  },

  save: function save(aContact) {
    // We have to do a deep copy of the contact manually here because
    // nsFrameMessageManager doesn't know how to create a structured clone of a
    // mozContact object.
    let newContact = {properties: {}};

    try {
      for (let field of PROPERTIES) {
        // This hack makes sure modifications to the sequence attributes get validated.
        aContact[field] = aContact[field];
        newContact.properties[field] = aContact[field];
      }
    } catch (e) {
      // And then make sure we throw a proper error message (no internal file and line #)
      throw new this._window.DOMError(e.name, e.message);
    }

    let request = this.createRequest();
    let requestID = this.getRequestId({request: request});

    let reason;
    if (aContact.id == "undefined") {
      // for example {25c00f01-90e5-c545-b4d4-21E2ddbab9e0} becomes
      // 25c00f0190e5c545b4d421E2ddbab9e0
      aContact.id = this._getRandomId().replace(/[{}-]/g, "");
      // Cache the contact so that its ID may be updated later if necessary
      this._cachedContacts[requestID] = aContact;
      reason = "create";
    } else {
      reason = "update";
    }

    newContact.id = aContact.id;
    newContact.published = aContact.published;
    newContact.updated = aContact.updated;

    if (DEBUG) debug("send: " + JSON.stringify(newContact));

    let options = { contact: newContact, reason: reason };
    let allowCallback = function() {
      cpmm.sendAsyncMessage("Contact:Save", {requestID: requestID, options: options});
    }.bind(this)
    this.askPermission(reason, request, allowCallback);
    return request;
  },

  find: function(aOptions) {
    if (DEBUG) debug("find! " + JSON.stringify(aOptions));
    let request = this.createRequest();
    let options = { findOptions: aOptions };
    let allowCallback = function() {
      cpmm.sendAsyncMessage("Contacts:Find", {requestID: this.getRequestId({request: request, reason: "find"}), options: options});
    }.bind(this)
    this.askPermission("find", request, allowCallback);
    return request;
  },

  createCursor: function CM_createCursor(aRequest) {
    let data = {
      cursor: Services.DOMRequest.createCursor(this._window, function() {
        this.handleContinue(id);
      }.bind(this)),
      cachedContacts: [],
      waitingForNext: true,
    };
    let id = this.getRequestId(data);
    if (DEBUG) debug("saved cursor id: " + id);
    return [id, data.cursor];
  },

  getAll: function CM_getAll(aOptions) {
    if (DEBUG) debug("getAll: " + JSON.stringify(aOptions));
    let [cursorId, cursor] = this.createCursor();
    let allowCallback = function() {
      cpmm.sendAsyncMessage("Contacts:GetAll", {
        cursorId: cursorId, findOptions: aOptions});
    }.bind(this);
    this.askPermission("find", cursor, allowCallback);
    return cursor;
  },

  nextTick: function nextTick(aCallback) {
    Services.tm.currentThread.dispatch(aCallback, Ci.nsIThread.DISPATCH_NORMAL);
  },

  handleContinue: function CM_handleContinue(aCursorId) {
    if (DEBUG) debug("handleContinue: " + aCursorId);
    let data = this.getRequest(aCursorId);
    if (data.cachedContacts.length > 0) {
      if (DEBUG) debug("contact in cache");
      let contact = data.cachedContacts.shift();
      this.nextTick(this._fireSuccessOrDone.bind(this, data.cursor, contact));
      if (!contact) {
        this.removeRequest(aCursorId);
      } else if (data.cachedContacts.length === CONTACTS_SENDMORE_MINIMUM) {
        cpmm.sendAsyncMessage("Contacts:GetAll:SendNow", { cursorId: aCursorId });
      }
    } else {
      if (DEBUG) debug("waiting for contact");
      data.waitingForNext = true;
    }
  },

  remove: function removeContact(aRecordOrId) {
    let request = this.createRequest();
    let id;
    if (typeof aRecordOrId === "string") {
      id = aRecordOrId;
    } else if (!aRecordOrId || !aRecordOrId.id) {
      Services.DOMRequest.fireErrorAsync(request, true);
      return request;
    } else {
      id = aRecordOrId.id;
    }

    let options = { id: id };
    let allowCallback = function() {
      cpmm.sendAsyncMessage("Contact:Remove", {requestID: this.getRequestId({request: request, reason: "remove"}), options: options});
    }.bind(this);
    this.askPermission("remove", request, allowCallback);
    return request;
  },

  clear: function() {
    if (DEBUG) debug("clear");
    let request = this.createRequest();
    let options = {};
    let allowCallback = function() {
      cpmm.sendAsyncMessage("Contacts:Clear", {requestID: this.getRequestId({request: request, reason: "remove"}), options: options});
    }.bind(this);
    this.askPermission("remove", request, allowCallback);
    return request;
  },

  getRevision: function() {
    let request = this.createRequest();

    let allowCallback = function() {
      cpmm.sendAsyncMessage("Contacts:GetRevision", {
        requestID: this.getRequestId({ request: request })
      });
    }.bind(this);

    let cancelCallback = function() {
      Services.DOMRequest.fireError(request, "");
    };

    this.askPermission("revision", request, allowCallback, cancelCallback);
    return request;
  },

  getCount: function() {
    let request = this.createRequest();

    let allowCallback = function() {
      cpmm.sendAsyncMessage("Contacts:GetCount", {
        requestID: this.getRequestId({ request: request })
      });
    }.bind(this);

    let cancelCallback = function() {
      Services.DOMRequest.fireError(request, "");
    };

    this.askPermission("count", request, allowCallback, cancelCallback);
    return request;
  },

  init: function(aWindow) {
    // DOMRequestIpcHelper.initHelper sets this._window
    this.initDOMRequestHelper(aWindow, ["Contacts:Find:Return:OK", "Contacts:Find:Return:KO",
                              "Contacts:Clear:Return:OK", "Contacts:Clear:Return:KO",
                              "Contact:Save:Return:OK", "Contact:Save:Return:KO",
                              "Contact:Remove:Return:OK", "Contact:Remove:Return:KO",
                              "Contact:Changed",
                              "Contacts:GetAll:Next", "Contacts:GetAll:Return:KO",
                              "Contacts:Count",
                              "Contacts:Revision", "Contacts:GetRevision:Return:KO",]);


    let allowCallback = function() {
      cpmm.sendAsyncMessage("Contacts:RegisterForMessages");
      this.hasListenPermission = true;
    }.bind(this);

    this.askPermission("listen", null, allowCallback);
  },

  classID: Components.ID("{8beb3a66-d70a-4111-b216-b8e995ad3aff}"),
  contractID: "@mozilla.org/contactManager;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver,
                                         Ci.nsIDOMGlobalPropertyInitializer]),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([
  Contact, ContactManager
]);
