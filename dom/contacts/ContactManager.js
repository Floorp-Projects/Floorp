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
Cu.import("resource://gre/modules/ObjectWrapper.jsm");

XPCOMUtils.defineLazyGetter(Services, "DOMRequest", function() {
  return Cc["@mozilla.org/dom/dom-request-service;1"].getService(Ci.nsIDOMRequestService);
});

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

const nsIClassInfo            = Ci.nsIClassInfo;
const CONTACTPROPERTIES_CID   = Components.ID("{f5181640-89e8-11e1-b0c4-0800200c9a66}");
const nsIDOMContactProperties = Ci.nsIDOMContactProperties;

// ContactProperties is not directly instantiated. It is used as interface.

function ContactProperties(aProp) { if (DEBUG) debug("ContactProperties Constructor"); }

ContactProperties.prototype = {

  classID : CONTACTPROPERTIES_CID,
  classInfo : XPCOMUtils.generateCI({classID: CONTACTPROPERTIES_CID,
                                     contractID:"@mozilla.org/contactProperties;1",
                                     classDescription: "ContactProperties",
                                     interfaces: [nsIDOMContactProperties],
                                     flags: nsIClassInfo.DOM_OBJECT}),

  QueryInterface : XPCOMUtils.generateQI([nsIDOMContactProperties])
}

//ContactAddress

const CONTACTADDRESS_CONTRACTID = "@mozilla.org/contactAddress;1";
const CONTACTADDRESS_CID        = Components.ID("{eba48030-89e8-11e1-b0c4-0800200c9a66}");
const nsIDOMContactAddress      = Components.interfaces.nsIDOMContactAddress;

function ContactAddress(aType, aStreetAddress, aLocality, aRegion, aPostalCode, aCountryName) {
  this.type = aType || null;
  this.streetAddress = aStreetAddress || null;
  this.locality = aLocality || null;
  this.region = aRegion || null;
  this.postalCode = aPostalCode || null;
  this.countryName = aCountryName || null;
};

ContactAddress.prototype = {
  __exposedProps__: {
                      type: 'rw',
                      streetAddress: 'rw',
                      locality: 'rw',
                      region: 'rw',
                      postalCode: 'rw',
                      countryName: 'rw'
                     },

  classID : CONTACTADDRESS_CID,
  classInfo : XPCOMUtils.generateCI({classID: CONTACTADDRESS_CID,
                                     contractID: CONTACTADDRESS_CONTRACTID,
                                     classDescription: "ContactAddress",
                                     interfaces: [nsIDOMContactAddress],
                                     flags: nsIClassInfo.DOM_OBJECT}),

  QueryInterface : XPCOMUtils.generateQI([nsIDOMContactAddress])
}

//ContactField

const CONTACTFIELD_CONTRACTID = "@mozilla.org/contactField;1";
const CONTACTFIELD_CID        = Components.ID("{e2cb19c0-e4aa-11e1-9b23-0800200c9a66}");
const nsIDOMContactField      = Components.interfaces.nsIDOMContactField;

function ContactField(aType, aValue) {
  this.type = aType || null;
  this.value = aValue || null;
};

ContactField.prototype = {
  __exposedProps__: {
                      type: 'rw',
                      value: 'rw'
                     },

  classID : CONTACTFIELD_CID,
  classInfo : XPCOMUtils.generateCI({classID: CONTACTFIELD_CID,
                                     contractID: CONTACTFIELD_CONTRACTID,
                                     classDescription: "ContactField",
                                     interfaces: [nsIDOMContactField],
                                     flags: nsIClassInfo.DOM_OBJECT}),

  QueryInterface : XPCOMUtils.generateQI([nsIDOMContactField])
}

//ContactTelField

const CONTACTTELFIELD_CONTRACTID = "@mozilla.org/contactTelField;1";
const CONTACTTELFIELD_CID        = Components.ID("{ed0ab260-e4aa-11e1-9b23-0800200c9a66}");
const nsIDOMContactTelField      = Components.interfaces.nsIDOMContactTelField;

function ContactTelField(aType, aValue, aCarrier) {
  this.type = aType || null;
  this.value = aValue || null;
  this.carrier = aCarrier || null;
};

ContactTelField.prototype = {
  __exposedProps__: {
                      type: 'rw',
                      value: 'rw',
                      carrier: 'rw'
                     },

  classID : CONTACTTELFIELD_CID,
  classInfo : XPCOMUtils.generateCI({classID: CONTACTTELFIELD_CID,
                                     contractID: CONTACTTELFIELD_CONTRACTID,
                                     classDescription: "ContactTelField",
                                     interfaces: [nsIDOMContactTelField],
                                     flags: nsIClassInfo.DOM_OBJECT}),

  QueryInterface : XPCOMUtils.generateQI([nsIDOMContactTelField])
}

//ContactFindSortOptions

const CONTACTFINDSORTOPTIONS_CONTRACTID = "@mozilla.org/contactFindSortOptions;1"
const CONTACTFINDSORTOPTIONS_CID        = Components.ID("{cb008c06-3bf8-495c-8865-f9ca1673a1e1}");
const nsIDOMContactFindSortOptions      = Ci.nsIDOMContactFindSortOptions;

function ContactFindSortOptions () { }

ContactFindSortOptions.prototype = {
  classID: CONTACTFINDSORTOPTIONS_CID,
  classInfo: XPCOMUtils.generateCI({classID: CONTACTFINDSORTOPTIONS_CID,
                                    contractID: CONTACTFINDSORTOPTIONS_CONTRACTID,
                                    classDescription: "ContactFindSortOptions",
                                    interfaces: [nsIDOMContactFindSortOptions],
                                    flags: nsIClassInfo.DOM_OBJECT}),
  QueryInterface: XPCOMUtils.generateQI([nsIDOMContactFindSortOptions])
};

//ContactFindOptions

const CONTACTFINDOPTIONS_CONTRACTID = "@mozilla.org/contactFindOptions;1";
const CONTACTFINDOPTIONS_CID        = Components.ID("{e31daea0-0cb6-11e1-be50-0800200c9a66}");
const nsIDOMContactFindOptions      = Components.interfaces.nsIDOMContactFindOptions;

function ContactFindOptions() { };

ContactFindOptions.prototype = {

  classID : CONTACTFINDOPTIONS_CID,
  classInfo : XPCOMUtils.generateCI({classID: CONTACTFINDOPTIONS_CID,
                                     contractID: CONTACTFINDOPTIONS_CONTRACTID,
                                     classDescription: "ContactFindOptions",
                                     interfaces: [nsIDOMContactFindSortOptions,
                                                  nsIDOMContactFindOptions],
                                     flags: nsIClassInfo.DOM_OBJECT}),

  QueryInterface : XPCOMUtils.generateQI([nsIDOMContactFindSortOptions,
                                          nsIDOMContactFindOptions])
}

//Contact

const CONTACT_CONTRACTID = "@mozilla.org/contact;1";
const CONTACT_CID        = Components.ID("{da0f7040-388b-11e1-b86c-0800200c9a66}");
const nsIDOMContact      = Components.interfaces.nsIDOMContact;

function Contact() { };

Contact.prototype = {
  __exposedProps__: {
                      id: 'rw',
                      updated: 'rw',
                      published:  'rw',
                      name: 'rw',
                      honorificPrefix: 'rw',
                      givenName: 'rw',
                      additionalName: 'rw',
                      familyName: 'rw',
                      honorificSuffix: 'rw',
                      nickname: 'rw',
                      email: 'rw',
                      photo: 'rw',
                      url: 'rw',
                      category: 'rw',
                      adr: 'rw',
                      tel: 'rw',
                      org: 'rw',
                      jobTitle: 'rw',
                      bday: 'rw',
                      note: 'rw',
                      impp: 'rw',
                      anniversary: 'rw',
                      sex: 'rw',
                      genderIdentity: 'rw'
                     },

  init: function init(aProp) {
    // Accept non-array strings for DOMString[] properties and convert them.
    function _create(aField) {
      if (Array.isArray(aField)) {
        for (let i = 0; i < aField.length; i++) {
          if (typeof aField[i] != "string")
            aField[i] = String(aField[i]);
        }
        return aField;
      } else if (aField != null) {
        return [String(aField)];
      }
    };

    function _checkBlobArray(aBlob) {
      if (Array.isArray(aBlob)) {
        for (let i = 0; i < aBlob.length; i++) {
          if (typeof aBlob != 'object') {
            return null;
          }
          if (!(aBlob[i] instanceof Components.interfaces.nsIDOMBlob)) {
            return null;
          }
        }
        return aBlob;
      }
      return null;
    };

    this.name =            _create(aProp.name) || null;
    this.honorificPrefix = _create(aProp.honorificPrefix) || null;
    this.givenName =       _create(aProp.givenName) || null;
    this.additionalName =  _create(aProp.additionalName) || null;
    this.familyName =      _create(aProp.familyName) || null;
    this.honorificSuffix = _create(aProp.honorificSuffix) || null;
    this.nickname =        _create(aProp.nickname) || null;

    if (aProp.email) {
      aProp.email = Array.isArray(aProp.email) ? aProp.email : [aProp.email];
      this.email = new Array();
      for (let i = 0; i < aProp.email.length; i++)
        this.email.push(new ContactField(aProp.email[i].type, aProp.email[i].value));
    } else {
      this.email = null;
    }

    this.photo =           _checkBlobArray(aProp.photo) || null;
    this.category =        _create(aProp.category) || null;

    if (aProp.adr) {
      // Make sure adr argument is an array. Instanceof doesn't work.
      aProp.adr = Array.isArray(aProp.adr) ? aProp.adr : [aProp.adr];

      this.adr = new Array();
      for (let i = 0; i < aProp.adr.length; i++)
        this.adr.push(new ContactAddress(aProp.adr[i].type, aProp.adr[i].streetAddress, aProp.adr[i].locality,
                                         aProp.adr[i].region, aProp.adr[i].postalCode, aProp.adr[i].countryName));
    } else {
      this.adr = null;
    }

    if (aProp.tel) {
      aProp.tel = Array.isArray(aProp.tel) ? aProp.tel : [aProp.tel];
      this.tel = new Array();
      for (let i = 0; i < aProp.tel.length; i++)
        this.tel.push(new ContactTelField(aProp.tel[i].type, aProp.tel[i].value, aProp.tel[i].carrier));
    } else {
      this.tel = null;
    }

    this.org =             _create(aProp.org) || null;
    this.jobTitle =        _create(aProp.jobTitle) || null;
    this.bday =            (aProp.bday == "undefined" || aProp.bday == null) ? null : new Date(aProp.bday);
    this.note =            _create(aProp.note) || null;

    if (aProp.impp) {
      aProp.impp = Array.isArray(aProp.impp) ? aProp.impp : [aProp.impp];
      this.impp = new Array();
      for (let i = 0; i < aProp.impp.length; i++)
        this.impp.push(new ContactField(aProp.impp[i].type, aProp.impp[i].value));
    } else {
      this.impp = null;
    }

    if (aProp.url) {
      aProp.url = Array.isArray(aProp.url) ? aProp.url : [aProp.url];
      this.url = new Array();
      for (let i = 0; i < aProp.url.length; i++)
        this.url.push(new ContactField(aProp.url[i].type, aProp.url[i].value));
    } else {
      this.url = null;
    }

    this.anniversary =     (aProp.anniversary == "undefined" || aProp.anniversary == null) ? null : new Date(aProp.anniversary);
    this.sex =             (aProp.sex != "undefined") ? aProp.sex : null;
    this.genderIdentity =  (aProp.genderIdentity != "undefined") ? aProp.genderIdentity : null;
  },

  get published () {
    return this._published;
  },

  set published(aPublished) {
    this._published = aPublished;
  },

  get updated () {
    return this._updated;
  },

  set updated(aUpdated) {
    this._updated = aUpdated;
  },

  classID : CONTACT_CID,
  classInfo : XPCOMUtils.generateCI({classID: CONTACT_CID,
                                     contractID: CONTACT_CONTRACTID,
                                     classDescription: "Contact",
                                     interfaces: [nsIDOMContact, nsIDOMContactProperties],
                                     flags: nsIClassInfo.DOM_OBJECT}),

  QueryInterface : XPCOMUtils.generateQI([nsIDOMContact, nsIDOMContactProperties])
}

// ContactManager

const CONTACTMANAGER_CONTRACTID = "@mozilla.org/contactManager;1";
const CONTACTMANAGER_CID        = Components.ID("{1d70322b-f11b-4f19-9586-7bf291f212aa}");
const nsIDOMContactManager      = Components.interfaces.nsIDOMContactManager;

function ContactManager()
{
  if (DEBUG) debug("Constructor");
}

ContactManager.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,
  _oncontactchange: null,

  _cursorData: {},

  set oncontactchange(aCallback) {
    if (DEBUG) debug("set oncontactchange");
    let allowCallback = function() {
      if (!this._oncontactchange) {
        cpmm.sendAsyncMessage("Contacts:RegisterForMessages");
      }
      this._oncontactchange = aCallback;
    }.bind(this);
    let cancelCallback = function() {
      throw Components.results.NS_ERROR_FAILURE;
    }
    this.askPermission("listen", null, allowCallback, cancelCallback);
  },

  get oncontactchange() {
    return this._oncontactchange;
  },

  _setMetaData: function(aNewContact, aRecord) {
    aNewContact.id = aRecord.id;
    aNewContact.published = aRecord.published;
    aNewContact.updated = aRecord.updated;
  },

  _convertContact: function CM_convertContact(aContact) {
    let newContact = new Contact();
    newContact.init(aContact.properties);
    this._setMetaData(newContact, aContact);
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
        let data = this._cursorData[msg.cursorId];
        let result = contacts ? this._convertContacts(contacts) : [null];
        if (data.waitingForNext) {
          if (DEBUG) debug("cursor waiting for contact, sending");
          data.waitingForNext = false;
          let contact = result.shift();
          this._pushArray(data.cachedContacts, result);
          this.nextTick(this._fireSuccessOrDone.bind(this, data.cursor, contact));
        } else {
          if (DEBUG) debug("cursor not waiting, saving");
          this._pushArray(data.cachedContacts, result);
        }
        break;
      case "Contacts:GetSimContacts:Return:OK":
        req = this.getRequest(msg.requestID);
        if (req) {
          let result = contacts.map(function(c) {
            let contact = new Contact();
            let prop = {name: [c.alphaId], tel: [ { value: c.number } ]};

            if (c.email) {
              prop.email = [{value: c.email}];
            }

            // ANR - Additional Number
            if (c.anr) {
              for (let i = 0; i < c.anr.length; i++) {
                prop.tel.push({value: c.anr[i]});
              }
            }

            contact.init(prop);
            return contact;
          });
          if (DEBUG) debug("result: " + JSON.stringify(result));
          Services.DOMRequest.fireSuccess(req.request,
                                          ObjectWrapper.wrap(result, this._window));
        } else {
          if (DEBUG) debug("no request stored!" + msg.requestID);
        }
        break;
      case "Contact:Save:Return:OK":
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
      case "Contacts:GetSimContacts:Return:KO":
        req = this.getRequest(msg.requestID);
        if (req)
          Services.DOMRequest.fireError(req.request, msg.errorMsg);
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
        if (this._oncontactchange) {
          let event = new this._window.MozContactChangeEvent("contactchanged", {
            contactID: msg.contactID,
            reason: msg.reason
          });
          this._oncontactchange.handleEvent(event);
        }
        break;
      default:
        if (DEBUG) debug("Wrong message: " + aMessage.name);
    }
    this.removeRequest(msg.requestID);
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
      case "getSimContacts":
      case "listen":
        access = "read";
        break;
      default:
        access = "unknown";
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
      browserFlag: principal.isInBrowserElement
    });
  },

  save: function save(aContact) {
    let request;
    if (DEBUG) debug("save: " + JSON.stringify(aContact) + " :" + aContact.id);
    let newContact = {};
    newContact.properties = {
      name:            [],
      honorificPrefix: [],
      givenName:       [],
      additionalName:  [],
      familyName:      [],
      honorificSuffix: [],
      nickname:        [],
      email:           [],
      photo:           [],
      url:             [],
      category:        [],
      adr:             [],
      tel:             [],
      org:             [],
      jobTitle:        [],
      bday:            null,
      note:            [],
      impp:            [],
      anniversary:     null,
      sex:             null,
      genderIdentity:  null
    };
    for (let field in newContact.properties) {
      newContact.properties[field] = aContact[field];
    }

    let reason;
    if (aContact.id == "undefined") {
      // for example {25c00f01-90e5-c545-b4d4-21E2ddbab9e0} becomes
      // 25c00f0190e5c545b4d421E2ddbab9e0
      aContact.id = this._getRandomId().replace('-', '', 'g').replace('{', '').replace('}', '');
      reason = "create";
    } else {
      reason = "update";
    }

    this._setMetaData(newContact, aContact);
    if (DEBUG) debug("send: " + JSON.stringify(newContact));
    request = this.createRequest();
    let options = { contact: newContact, reason: reason };
    let allowCallback = function() {
      cpmm.sendAsyncMessage("Contact:Save", {requestID: this.getRequestId({request: request, reason: reason}), options: options});
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
    let id = this._getRandomId();
    let data = {
      cursor: Services.DOMRequest.createCursor(this._window, function() {
        this.handleContinue(id);
      }.bind(this)),
      cachedContacts: [],
      waitingForNext: true,
    };
    if (DEBUG) debug("saved cursor id: " + id);
    this._cursorData[id] = data;
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
    let data = this._cursorData[aCursorId];
    if (data.cachedContacts.length > 0) {
      if (DEBUG) debug("contact in cache");
      let contact = data.cachedContacts.shift();
      this.nextTick(this._fireSuccessOrDone.bind(this, data.cursor, contact));
    } else {
      if (DEBUG) debug("waiting for contact");
      data.waitingForNext = true;
    }
  },

  remove: function removeContact(aRecord) {
    let request;
    request = this.createRequest();
    let options = { id: aRecord.id };
    let allowCallback = function() {
      cpmm.sendAsyncMessage("Contact:Remove", {requestID: this.getRequestId({request: request, reason: "remove"}), options: options});
    }.bind(this)
    this.askPermission("remove", request, allowCallback);
    return request;
  },

  clear: function() {
    if (DEBUG) debug("clear");
    let request;
    request = this.createRequest();
    let options = {};
    let allowCallback = function() {
      cpmm.sendAsyncMessage("Contacts:Clear", {requestID: this.getRequestId({request: request, reason: "remove"}), options: options});
    }.bind(this)
    this.askPermission("remove", request, allowCallback);
    return request;
  },

  getSimContacts: function(aContactType) {
    let request;
    request = this.createRequest();
    let options = {contactType: aContactType};

    let allowCallback = function() {
      if (DEBUG) debug("getSimContacts " + aContactType);
      cpmm.sendAsyncMessage("Contacts:GetSimContacts",
        {requestID: this.getRequestId({request: request, reason: "getSimContacts"}),
         options: options});
    }.bind(this);

    let cancelCallback = function() {
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    }
    this.askPermission("getSimContacts", request, allowCallback, cancelCallback);
    return request;
  },

  init: function(aWindow) {
    // Set navigator.mozContacts to null.
    if (!Services.prefs.getBoolPref("dom.mozContacts.enabled"))
      return null;

    this.initHelper(aWindow, ["Contacts:Find:Return:OK", "Contacts:Find:Return:KO",
                              "Contacts:Clear:Return:OK", "Contacts:Clear:Return:KO",
                              "Contact:Save:Return:OK", "Contact:Save:Return:KO",
                              "Contact:Remove:Return:OK", "Contact:Remove:Return:KO",
                              "Contacts:GetSimContacts:Return:OK",
                              "Contacts:GetSimContacts:Return:KO",
                              "Contact:Changed",
                              "PermissionPromptHelper:AskPermission:OK",
                              "Contacts:GetAll:Next"]);
  },

  // Called from DOMRequestIpcHelper
  uninit: function uninit() {
    if (DEBUG) debug("uninit call");
    if (this._oncontactchange)
      this._oncontactchange = null;
  },

  classID : CONTACTMANAGER_CID,
  QueryInterface : XPCOMUtils.generateQI([nsIDOMContactManager, Ci.nsIDOMGlobalPropertyInitializer]),

  classInfo : XPCOMUtils.generateCI({classID: CONTACTMANAGER_CID,
                                     contractID: CONTACTMANAGER_CONTRACTID,
                                     classDescription: "ContactManager",
                                     interfaces: [nsIDOMContactManager],
                                     flags: nsIClassInfo.DOM_OBJECT})
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory(
                       [Contact, ContactManager, ContactProperties, ContactAddress, ContactField, ContactTelField, ContactFindSortOptions, ContactFindOptions])
