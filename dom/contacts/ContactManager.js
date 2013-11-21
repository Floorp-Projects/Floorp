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

XPCOMUtils.defineLazyServiceGetter(this, "pm",
                                   "@mozilla.org/permissionmanager;1",
                                   "nsIPermissionManager");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

const CONTACTS_SENDMORE_MINIMUM = 5;

function ContactAddressImpl() { }

ContactAddressImpl.prototype = {
  // This function is meant to be called via bindings code for type checking,
  // don't call it directly. Instead, create a content object and call initialize
  // on that.
  initialize: function(aType, aStreetAddress, aLocality, aRegion, aPostalCode, aCountryName, aPref) {
    this.type = aType;
    this.streetAddress = aStreetAddress;
    this.locality = aLocality;
    this.region = aRegion;
    this.postalCode = aPostalCode;
    this.countryName = aCountryName;
    this.pref = aPref;
  },

  toJSON: function(excludeExposedProps) {
    let json = {
      type: this.type,
      streetAddress: this.streetAddress,
      locality: this.locality,
      region: this.region,
      postalCode: this.postalCode,
      countryName: this.countryName,
      pref: this.pref,
    };
    if (!excludeExposedProps) {
      json.__exposedProps__ = {
        type: "rw",
        streetAddress: "rw",
        locality: "rw",
        region: "rw",
        postalCode: "rw",
        countryName: "rw",
        pref: "rw",
      };
    }
    return json;
  },

  classID: Components.ID("{9cbfa81c-bcab-4ca9-b0d2-f4318f295e33}"),
  contractID: "@mozilla.org/contactAddress;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports]),
};

function ContactFieldImpl() { }

ContactFieldImpl.prototype = {
  // This function is meant to be called via bindings code for type checking,
  // don't call it directly. Instead, create a content object and call initialize
  // on that.
  initialize: function(aType, aValue, aPref) {
    this.type = aType;
    this.value = aValue;
    this.pref = aPref;
  },

  toJSON: function(excludeExposedProps) {
    let json = {
      type: this.type,
      value: this.value,
      pref: this.pref,
    };
    if (!excludeExposedProps) {
      json.__exposedProps__ = {
        type: "rw",
        value: "rw",
        pref: "rw",
      };
    }
    return json;
  },

  classID: Components.ID("{ad19a543-69e4-44f0-adfa-37c011556bc1}"),
  contractID: "@mozilla.org/contactField;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports]),
};

function ContactTelFieldImpl() { }

ContactTelFieldImpl.prototype = {
  // This function is meant to be called via bindings code for type checking,
  // don't call it directly. Instead, create a content object and call initialize
  // on that.
  initialize: function(aType, aValue, aCarrier, aPref) {
    this.type = aType;
    this.value = aValue;
    this.carrier = aCarrier;
    this.pref = aPref;
  },

  toJSON: function(excludeExposedProps) {
    let json = {
      type: this.type,
      value: this.value,
      carrier: this.carrier,
      pref: this.pref,
    };
    if (!excludeExposedProps) {
      json.__exposedProps__ = {
        type: "rw",
        value: "rw",
        carrier: "rw",
        pref: "rw",
      };
    }
    return json;
  },

  classID: Components.ID("{4d42c5a9-ea5d-4102-80c3-40cc986367ca}"),
  contractID: "@mozilla.org/contactTelField;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports]),
};

function validateArrayField(data, createCb) {
  // We use an array-like Proxy to validate data set by content, since we don't
  // have WebIDL arrays yet. See bug 851726.

  // ArrayPropertyExposedPropsProxy is used to return "rw" for any valid index
  // and "length" in __exposedProps__.
  const ArrayPropertyExposedPropsProxy = new Proxy({}, {
    get: function(target, name) {
      // Test for index access
      if (String(name >>> 0) === name) {
        return "rw";
      }
      if (name === "length") {
        return "r";
      }
    }
  });

  const ArrayPropertyHandler = {
    set: function(target, name, val, receiver) {
      // Test for index access
      if (String(name >>> 0) === name) {
        target[name] = createCb(val);
      }
    },
    get: function(target, name) {
      if (name === "__exposedProps__") {
        return ArrayPropertyExposedPropsProxy;
      }
      return target[name];
    }
  };

  if (data === null || data === undefined) {
    return undefined;
  }

  data = Array.isArray(data) ? data : [data];
  let filtered = [];
  for (let i = 0, n = data.length; i < n; ++i) {
    if (data[i]) {
      filtered.push(createCb(data[i]));
    }
  }
  return new Proxy(filtered, ArrayPropertyHandler);
}

// We need this to create a copy of the mozContact object in ContactManager.save
// Keep in sync with the interfaces.
const PROPERTIES = [
  "name", "honorificPrefix", "givenName", "additionalName", "familyName",
  "honorificSuffix", "nickname", "photo", "category", "org", "jobTitle",
  "bday", "note", "anniversary", "sex", "genderIdentity", "key"
];
const ADDRESS_PROPERTIES = ["adr"];
const FIELD_PROPERTIES = ["email", "url", "impp"];
const TELFIELD_PROPERTIES = ["tel"];

function Contact() { }

Contact.prototype = {
  // We need to create the content interfaces in these setters, otherwise when
  // we return these objects (e.g. from a find call), the values in the array
  // will be COW's, and content cannot see the properties.
  set email(aEmail) {
    this._email = aEmail;
  },

  get email() {
    this._email = validateArrayField(this._email, function(email) {
      let obj = this._window.ContactField._create(this._window, new ContactFieldImpl());
      obj.initialize(email.type, email.value, email.pref);
      return obj;
    }.bind(this));
    return this._email;
  },

  set adr(aAdr) {
    this._adr = aAdr;
  },

  get adr() {
    this._adr = validateArrayField(this._adr, function(adr) {
      let obj = this._window.ContactAddress._create(this._window, new ContactAddressImpl());
      obj.initialize(adr.type, adr.streetAddress, adr.locality,
                     adr.region, adr.postalCode, adr.countryName,
                     adr.pref);
      return obj;
    }.bind(this));
    return this._adr;
  },

  set tel(aTel) {
    this._tel = aTel;
  },

  get tel() {
    this._tel = validateArrayField(this._tel, function(tel) {
      let obj = this._window.ContactTelField._create(this._window, new ContactTelFieldImpl());
      obj.initialize(tel.type, tel.value, tel.carrier, tel.pref);
      return obj;
    }.bind(this));
    return this._tel;
  },

  set impp(aImpp) {
    this._impp = aImpp;
  },

  get impp() {
    this._impp = validateArrayField(this._impp, function(impp) {
      let obj = this._window.ContactField._create(this._window, new ContactFieldImpl());
      obj.initialize(impp.type, impp.value, impp.pref);
      return obj;
    }.bind(this));
    return this._impp;
  },

  set url(aUrl) {
    this._url = aUrl;
  },

  get url() {
    this._url = validateArrayField(this._url, function(url) {
      let obj = this._window.ContactField._create(this._window, new ContactFieldImpl());
      obj.initialize(url.type, url.value, url.pref);
      return obj;
    }.bind(this));
    return this._url;
  },

  init: function(aWindow) {
    this._window = aWindow;
  },

  __init: function(aProp) {
    this.name =            aProp.name;
    this.honorificPrefix = aProp.honorificPrefix;
    this.givenName =       aProp.givenName;
    this.additionalName =  aProp.additionalName;
    this.familyName =      aProp.familyName;
    this.honorificSuffix = aProp.honorificSuffix;
    this.nickname =        aProp.nickname;
    this.email =           aProp.email;
    this.photo =           aProp.photo;
    this.url =             aProp.url;
    this.category =        aProp.category;
    this.adr =             aProp.adr;
    this.tel =             aProp.tel;
    this.org =             aProp.org;
    this.jobTitle =        aProp.jobTitle;
    this.bday =            aProp.bday;
    this.note =            aProp.note;
    this.impp =            aProp.impp;
    this.anniversary =     aProp.anniversary;
    this.sex =             aProp.sex;
    this.genderIdentity =  aProp.genderIdentity;
    this.key =             aProp.key;
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

  toJSON: function() {
    return {
      id:              this.id,
      published:       this.published,
      updated:         this.updated,

      name:            this.name,
      honorificPrefix: this.honorificPrefix,
      givenName:       this.givenName,
      additionalName:  this.additionalName,
      familyName:      this.familyName,
      honorificSuffix: this.honorificSuffix,
      nickname:        this.nickname,
      category:        this.category,
      org:             this.org,
      jobTitle:        this.jobTitle,
      note:            this.note,
      sex:             this.sex,
      genderIdentity:  this.genderIdentity,
      email:           this.email,
      photo:           this.photo,
      adr:             this.adr,
      url:             this.url,
      tel:             this.tel,
      bday:            this.bday,
      impp:            this.impp,
      anniversary:     this.anniversary,
      key:             this.key,

      __exposedProps__: {
        id:              "rw",
        published:       "rw",
        updated:         "rw",
        name:            "rw",
        honorificPrefix: "rw",
        givenName:       "rw",
        additionalName:  "rw",
        familyName:      "rw",
        honorificSuffix: "rw",
        nickname:        "rw",
        category:        "rw",
        org:             "rw",
        jobTitle:        "rw",
        note:            "rw",
        sex:             "rw",
        genderIdentity:  "rw",
        email:           "rw",
        photo:           "rw",
        adr:             "rw",
        url:             "rw",
        tel:             "rw",
        bday:            "rw",
        impp:            "rw",
        anniversary:     "rw",
        key:             "rw",
      }
    };
  },

  classID: Components.ID("{72a5ee28-81d8-4af8-90b3-ae935396cc66}"),
  contractID: "@mozilla.org/contact;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer]),
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
    let contacts = [];
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
      case "PermissionPromptHelper:AskPermission:OK":
        if (DEBUG) debug("id: " + msg.requestID);
        req = this.getRequest(msg.requestID);
        if (!req) {
          break;
        }

        if (msg.result == Ci.nsIPermissionManager.ALLOW_ACTION) {
          req.allow();
        } else {
          req.cancel();
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
    let type = "contacts-" + access;
    let permValue =
      Services.perms.testExactPermissionFromPrincipal(this._window.document.nodePrincipal, type);
    if (permValue == Ci.nsIPermissionManager.ALLOW_ACTION) {
      aAllowCallback();
      return;
    }

    let requestID = this.getRequestId({
      request: aRequest,
      allow: function() {
        aAllowCallback();
      }.bind(this),
      cancel : function() {
        if (aCancelCallback) {
          aCancelCallback()
        } else if (aRequest) {
          Services.DOMRequest.fireError(aRequest, "Not Allowed");
        }
      }.bind(this)
    });

    let principal = this._window.document.nodePrincipal;
    cpmm.sendAsyncMessage("PermissionPromptHelper:AskPermission", {
      type: "contacts",
      access: access,
      requestID: requestID,
      origin: principal.origin,
      appID: principal.appId,
      browserFlag: principal.isInBrowserElement,
      windowID: this._window.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).outerWindowID
    });
  },

  save: function save(aContact) {
    // We have to do a deep copy of the contact manually here because
    // nsFrameMessageManager doesn't know how to create a structured clone of a
    // mozContact object.
    let newContact = {properties: {}};

    for (let field of PROPERTIES) {
      if (aContact[field]) {
        newContact.properties[field] = aContact[field];
      }
    }

    for (let prop of ADDRESS_PROPERTIES) {
      if (aContact[prop]) {
        newContact.properties[prop] = [];
        for (let i of aContact[prop]) {
          if (i) {
            let json = ContactAddressImpl.prototype.toJSON.apply(i, [true]);
            newContact.properties[prop].push(json);
          }
        }
      }
    }

    for (let prop of FIELD_PROPERTIES) {
      if (aContact[prop]) {
        newContact.properties[prop] = [];
        for (let i of aContact[prop]) {
          if (i) {
            let json = ContactFieldImpl.prototype.toJSON.apply(i, [true]);
            newContact.properties[prop].push(json);
          }
        }
      }
    }

    for (let prop of TELFIELD_PROPERTIES) {
      if (aContact[prop]) {
        newContact.properties[prop] = [];
        for (let i of aContact[prop]) {
          if (i) {
            let json = ContactTelFieldImpl.prototype.toJSON.apply(i, [true]);
            newContact.properties[prop].push(json);
          }
        }
      }
    }

    let request = this.createRequest();
    let requestID = this.getRequestId({request: request, reason: reason});

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

  remove: function removeContact(aRecord) {
    let request = this.createRequest();
    if (!aRecord || !aRecord.id) {
      Services.DOMRequest.fireErrorAsync(request, true);
      return request;
    }

    let options = { id: aRecord.id };
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
      Services.DOMRequest.fireError(request);
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
      Services.DOMRequest.fireError(request);
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
                              "PermissionPromptHelper:AskPermission:OK",
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
  Contact, ContactManager, ContactFieldImpl, ContactAddressImpl, ContactTelFieldImpl
]);
