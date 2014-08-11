/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Copyright © 2013, Deutsche Telekom, Inc. */

"use strict";

const DEBUG = false;
function debug(s) {
  if (DEBUG) dump("-*- Nfc DOM: " + s + "\n");
}

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this,
                                   "appsService",
                                   "@mozilla.org/AppsService;1",
                                   "nsIAppsService");
const NFC_PEER_EVENT_READY = 0x01;
const NFC_PEER_EVENT_LOST  = 0x02;

/**
 * NFCTag
 */
function MozNFCTag() {
  debug("In MozNFCTag Constructor");
  this._nfcContentHelper = Cc["@mozilla.org/nfc/content-helper;1"]
                             .getService(Ci.nsINfcContentHelper);
  this.session = null;

  // Map WebIDL declared enum map names to integer
  this._techTypesMap = [];
  this._techTypesMap['NFC_A'] = 0;
  this._techTypesMap['NFC_B'] = 1;
  this._techTypesMap['NFC_ISO_DEP'] = 2;
  this._techTypesMap['NFC_F'] = 3;
  this._techTypesMap['NFC_V'] = 4;
  this._techTypesMap['NDEF'] = 5;
  this._techTypesMap['NDEF_FORMATABLE'] = 6;
  this._techTypesMap['MIFARE_CLASSIC'] = 7;
  this._techTypesMap['MIFARE_ULTRALIGHT'] = 8;
  this._techTypesMap['NFC_BARCODE'] = 9;
  this._techTypesMap['P2P'] = 10;
}
MozNFCTag.prototype = {
  _nfcContentHelper: null,
  _window: null,

  initialize: function(aWindow, aSessionToken) {
    this._window = aWindow;
    this.session = aSessionToken;
  },

  _techTypesMap: null,

  // NFCTag interface:
  getDetailsNDEF: function getDetailsNDEF() {
    return this._nfcContentHelper.getDetailsNDEF(this._window, this.session);
  },
  readNDEF: function readNDEF() {
    return this._nfcContentHelper.readNDEF(this._window, this.session);
  },
  writeNDEF: function writeNDEF(records) {
    return this._nfcContentHelper.writeNDEF(this._window, records, this.session);
  },
  makeReadOnlyNDEF: function makeReadOnlyNDEF() {
    return this._nfcContentHelper.makeReadOnlyNDEF(this._window, this.session);
  },
  connect: function connect(enum_tech_type) {
    let int_tech_type = this._techTypesMap[enum_tech_type];
    return this._nfcContentHelper.connect(this._window, int_tech_type, this.session);
  },
  close: function close() {
    return this._nfcContentHelper.close(this._window, this.session);
  },

  classID: Components.ID("{4e1e2e90-3137-11e3-aa6e-0800200c9a66}"),
  contractID: "@mozilla.org/nfc/NFCTag;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer]),
};

/**
 * NFCPeer
 */
function MozNFCPeer() {
  debug("In MozNFCPeer Constructor");
  this._nfcContentHelper = Cc["@mozilla.org/nfc/content-helper;1"]
                             .getService(Ci.nsINfcContentHelper);
  this.session = null;
}
MozNFCPeer.prototype = {
  _nfcContentHelper: null,
  _window: null,
  _isLost: false,

  initialize: function(aWindow, aSessionToken) {
    this._window = aWindow;
    this.session = aSessionToken;
  },

  // NFCPeer interface:
  sendNDEF: function sendNDEF(records) {
    if (this._isLost) {
      throw new this._window.DOMError("InvalidStateError", "NFCPeer object is invalid");
    }

    // Just forward sendNDEF to writeNDEF
    return this._nfcContentHelper.writeNDEF(this._window, records, this.session);
  },

  sendFile: function sendFile(blob) {
    if (this._isLost) {
      throw new this._window.DOMError("InvalidStateError", "NFCPeer object is invalid");
    }

    let data = {
      "blob": blob
    };
    return this._nfcContentHelper.sendFile(this._window,
                                           Cu.cloneInto(data, this._window),
                                           this.session);
  },

  invalidate: function invalidate() {
    this._isLost = true;
  },

  classID: Components.ID("{c1b2bcf0-35eb-11e3-aa6e-0800200c9a66}"),
  contractID: "@mozilla.org/nfc/NFCPeer;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer]),
};

/**
 * Navigator NFC object
 */
function mozNfc() {
  debug("In mozNfc Constructor");
  try {
    this._nfcContentHelper = Cc["@mozilla.org/nfc/content-helper;1"]
                               .getService(Ci.nsINfcContentHelper);
  } catch(e) {
    debug("No NFC support.")
  }

  this._nfcContentHelper.registerPeerEventListener(this);
}
mozNfc.prototype = {
  _nfcContentHelper: null,
  _window: null,
  nfcObject: null,

  _wrap: function _wrap(obj) {
    return Cu.cloneInto(obj, this._window);
  },

  init: function init(aWindow) {
    debug("mozNfc init called");
    this._window = aWindow;
  },

  // Only apps which have nfc-manager permission can call the following interfaces
  // 'checkP2PRegistration' , 'notifyUserAcceptedP2P' , 'notifySendFileStatus',
  // 'startPoll', 'stopPoll', and 'powerOff'.
  checkP2PRegistration: function checkP2PRegistration(manifestUrl) {
    // Get the AppID and pass it to ContentHelper
    let appID = appsService.getAppLocalIdByManifestURL(manifestUrl);
    return this._nfcContentHelper.checkP2PRegistration(this._window, appID);
  },

  notifyUserAcceptedP2P: function notifyUserAcceptedP2P(manifestUrl) {
    let appID = appsService.getAppLocalIdByManifestURL(manifestUrl);
    // Notify chrome process of user's acknowledgement
    this._nfcContentHelper.notifyUserAcceptedP2P(this._window, appID);
  },

  notifySendFileStatus: function notifySendFileStatus(status, requestId) {
    this._nfcContentHelper.notifySendFileStatus(this._window,
                                                status, requestId);
  },

  startPoll: function startPoll() {
    return this._nfcContentHelper.startPoll(this._window);
  },

  stopPoll: function stopPoll() {
    return this._nfcContentHelper.stopPoll(this._window);
  },

  powerOff: function powerOff() {
    return this._nfcContentHelper.powerOff(this._window);
  },

  getNFCTag: function getNFCTag(sessionToken) {
    let obj = new MozNFCTag();
    obj.initialize(this._window, sessionToken);
    if (this._nfcContentHelper.setSessionToken(sessionToken)) {
      return this._window.MozNFCTag._create(this._window, obj);
    }
    return null;
  },

  getNFCPeer: function getNFCPeer(sessionToken) {
    if (!sessionToken || !this._nfcContentHelper.setSessionToken(sessionToken)) {
      return null;
    }

    if (!this.nfcObject || this.nfcObject.session != sessionToken) {
      let obj = new MozNFCPeer();
      obj.initialize(this._window, sessionToken);
      this.nfcObject = obj;
      this.nfcObject.contentObject = this._window.MozNFCPeer._create(this._window, obj);
    }

    return this.nfcObject.contentObject;
  },

  // get/set onpeerready
  get onpeerready() {
    return this.__DOM_IMPL__.getEventHandler("onpeerready");
  },

  set onpeerready(handler) {
    this.__DOM_IMPL__.setEventHandler("onpeerready", handler);
  },

  // get/set onpeerlost
  get onpeerlost() {
    return this.__DOM_IMPL__.getEventHandler("onpeerlost");
  },

  set onpeerlost(handler) {
    this.__DOM_IMPL__.setEventHandler("onpeerlost", handler);
  },

  eventListenerWasAdded: function(evt) {
    let eventType = this.getEventType(evt);
    if (eventType != NFC_PEER_EVENT_READY) {
      return;
    }

    let appId = this._window.document.nodePrincipal.appId;
    this._nfcContentHelper.registerTargetForPeerReady(this._window, appId);
  },

  eventListenerWasRemoved: function(evt) {
    let eventType = this.getEventType(evt);
    if (eventType != NFC_PEER_EVENT_READY) {
      return;
    }

    let appId = this._window.document.nodePrincipal.appId;
    this._nfcContentHelper.unregisterTargetForPeerReady(this._window, appId);
  },

  notifyPeerReady: function notifyPeerReady(sessionToken) {
    if (this.hasDeadWrapper()) {
      dump("this._window or this.__DOM_IMPL__ is a dead wrapper.");
      return;
    }

    this.session = sessionToken;

    debug("fire onpeerready sessionToken : " + sessionToken);
    let detail = {
      "detail":sessionToken
    };
    let event = new this._window.CustomEvent("peerready", this._wrap(detail));
    this.__DOM_IMPL__.dispatchEvent(event);
  },

  notifyPeerLost: function notifyPeerLost(sessionToken) {
    if (this.hasDeadWrapper()) {
      dump("this._window or this.__DOM_IMPL__ is a dead wrapper.");
      return;
    }

    if (sessionToken != this.session) {
      dump("Unpaired session for notifyPeerLost." + sessionToken);
      return;
    }

    if (this.nfcObject && (this.nfcObject.session == sessionToken)) {
      this.nfcObject.invalidate();
      this.nfcObject = null;
    }

    this.session = null;

    debug("fire onpeerlost");
    let event = new this._window.Event("peerlost");
    this.__DOM_IMPL__.dispatchEvent(event);
  },

  getEventType: function getEventType(evt) {
    let eventType = -1;
    switch (evt) {
      case 'peerready':
        eventType = NFC_PEER_EVENT_READY;
        break;
      case 'peerlost':
        eventType = NFC_PEER_EVENT_LOST;
        break;
      default:
        break;
    }
    return eventType;
  },

  hasDeadWrapper: function hasDeadWrapper() {
    return Cu.isDeadWrapper(this._window) || Cu.isDeadWrapper(this.__DOM_IMPL__);
  },

  classID: Components.ID("{6ff2b290-2573-11e3-8224-0800200c9a66}"),
  contractID: "@mozilla.org/navigatorNfc;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer,
                                         Ci.nsINfcPeerEventListener]),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([MozNFCTag, MozNFCPeer, mozNfc]);
