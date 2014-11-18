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

/**
 * Implementation of NFCTag.
 *
 * @param window  global window object.
 * @param sessionToken  session token received from parent process.
 * @parem event   type of nsINfcTagEvent received from parent process.
 */
function MozNFCTagImpl(window, sessionToken, event) {
  debug("In MozNFCTagImpl Constructor");
  this._nfcContentHelper = Cc["@mozilla.org/nfc/content-helper;1"]
                             .getService(Ci.nsINfcContentHelper);
  this._window = window;
  this.session = sessionToken;
  this.techList = event.techList;
  this.type = event.tagType || null;
  this.maxNDEFSize = event.maxNDEFSize || null;
  this.isReadOnly = event.isReadOnly || null;
  this.isFormatable = event.isFormatable || null;
  this.canBeMadeReadOnly = this.type ?
                             (this.type == "type1" || this.type == "type2" ||
                              this.type == "mifare_classic") :
                             null;
}
MozNFCTagImpl.prototype = {
  _nfcContentHelper: null,
  _window: null,
  session: null,
  techList: null,
  type: null,
  maxNDEFSize: 0,
  isReadOnly: false,
  isFormatable: false,
  canBeMadeReadOnly: false,
  isLost: false,

  // NFCTag interface:
  readNDEF: function readNDEF() {
    if (this.isLost) {
      throw new this._window.DOMError("InvalidStateError", "NFCTag object is invalid");
    }

    return this._nfcContentHelper.readNDEF(this.session);
  },

  writeNDEF: function writeNDEF(records) {
    if (this.isLost) {
      throw new this._window.DOMError("InvalidStateError", "NFCTag object is invalid");
    }

    if (this.isReadOnly) {
      throw new this._window.DOMError("InvalidAccessError", "NFCTag object is read-only");
    }

    let ndefLen = 0;
    for (let record of records) {
      ndefLen += record.size;
    }

    if (ndefLen > this.maxNDEFSize) {
      throw new this._window.DOMError("NotSupportedError", "Exceed max NDEF size");
    }

    return this._nfcContentHelper.writeNDEF(records, this.session);
  },

  makeReadOnly: function makeReadOnly() {
    if (this.isLost) {
      throw new this._window.DOMError("InvalidStateError", "NFCTag object is invalid");
    }

    if (!this.canBeMadeReadOnly) {
      throw new this._window.DOMError("InvalidAccessError",
                                      "NFCTag object cannot be made read-only");
    }

    return this._nfcContentHelper.makeReadOnly(this.session);
  },

  format: function format() {
    if (this.isLost) {
      throw new this._window.DOMError("InvalidStateError", "NFCTag object is invalid");
    }

    if (!this.isFormatable) {
      throw new this._window.DOMError("InvalidAccessError",
                                      "NFCTag object is not formatable");
    }

    return this._nfcContentHelper.format(this.session);
  },

  classID: Components.ID("{4e1e2e90-3137-11e3-aa6e-0800200c9a66}"),
  contractID: "@mozilla.org/nfc/NFCTag;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer]),
};

/**
 * Implementation of NFCPeer.
 *
 * @param window  global window object.
 * @param sessionToken  session token received from parent process.
 */
function MozNFCPeerImpl(aWindow, aSessionToken) {
  debug("In MozNFCPeerImpl Constructor");
  this._nfcContentHelper = Cc["@mozilla.org/nfc/content-helper;1"]
                             .getService(Ci.nsINfcContentHelper);

  this._window = aWindow;
  this.session = aSessionToken;
}
MozNFCPeerImpl.prototype = {
  _nfcContentHelper: null,
  _window: null,
  isLost: false,

  // NFCPeer interface:
  sendNDEF: function sendNDEF(records) {
    if (this.isLost) {
      throw new this._window.DOMError("InvalidStateError", "NFCPeer object is invalid");
    }

    // Just forward sendNDEF to writeNDEF
    return this._nfcContentHelper.writeNDEF(records, this.session);
  },

  sendFile: function sendFile(blob) {
    if (this.isLost) {
      throw new this._window.DOMError("InvalidStateError", "NFCPeer object is invalid");
    }

    let data = {
      "blob": blob
    };
    return this._nfcContentHelper.sendFile(Cu.cloneInto(data, this._window),
                                           this.session);
  },

  classID: Components.ID("{c1b2bcf0-35eb-11e3-aa6e-0800200c9a66}"),
  contractID: "@mozilla.org/nfc/NFCPeer;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer]),
};

/**
 * Implementation of navigator NFC object.
 */
function MozNFCImpl() {
  debug("In MozNFCImpl Constructor");
  try {
    this._nfcContentHelper = Cc["@mozilla.org/nfc/content-helper;1"]
                               .getService(Ci.nsINfcContentHelper);
  } catch(e) {
    debug("No NFC support.")
  }

  this._nfcContentHelper.addEventListener(this);
}
MozNFCImpl.prototype = {
  _nfcContentHelper: null,
  _window: null,
  nfcPeer: null,
  nfcTag: null,

  // Should be mapped to the RFState defined in WebIDL.
  rfState: {
    IDLE: "idle",
    LISTEN: "listen",
    DISCOVERY: "discovery"
  },

  init: function init(aWindow) {
    debug("MozNFCImpl init called");
    this._window = aWindow;
    this.defineEventHandlerGetterSetter("ontagfound");
    this.defineEventHandlerGetterSetter("ontaglost");
    this.defineEventHandlerGetterSetter("onpeerready");
    this.defineEventHandlerGetterSetter("onpeerfound");
    this.defineEventHandlerGetterSetter("onpeerlost");

    if (this._nfcContentHelper) {
      this._nfcContentHelper.init(aWindow);
    }
  },

  // Only apps which have nfc-manager permission can call the following interfaces
  // 'checkP2PRegistration' , 'notifyUserAcceptedP2P' , 'notifySendFileStatus',
  // 'startPoll', 'stopPoll', and 'powerOff'.
  checkP2PRegistration: function checkP2PRegistration(manifestUrl) {
    // Get the AppID and pass it to ContentHelper
    let appID = appsService.getAppLocalIdByManifestURL(manifestUrl);
    return this._nfcContentHelper.checkP2PRegistration(appID);
  },

  notifyUserAcceptedP2P: function notifyUserAcceptedP2P(manifestUrl) {
    let appID = appsService.getAppLocalIdByManifestURL(manifestUrl);
    // Notify chrome process of user's acknowledgement
    this._nfcContentHelper.notifyUserAcceptedP2P(appID);
  },

  notifySendFileStatus: function notifySendFileStatus(status, requestId) {
    this._nfcContentHelper.notifySendFileStatus(status, requestId);
  },

  startPoll: function startPoll() {
    return this._nfcContentHelper.changeRFState(this.rfState.DISCOVERY);
  },

  stopPoll: function stopPoll() {
    return this._nfcContentHelper.changeRFState(this.rfState.LISTEN);
  },

  powerOff: function powerOff() {
    return this._nfcContentHelper.changeRFState(this.rfState.IDLE);
  },

  _createNFCPeer: function _createNFCPeer(sessionToken) {
    let peer = new MozNFCPeerImpl(this._window, sessionToken);
    return this._window.MozNFCPeer._create(this._window, peer);
  },

  getNFCPeer: function getNFCPeer(sessionToken) {
    if (!sessionToken || !this._nfcContentHelper.checkSessionToken(sessionToken, true)) {
      return null;
    }

    if (!this.nfcPeer || this.nfcPeer.session != sessionToken) {
      this.nfcPeer = this._createNFCPeer(sessionToken);
    }

    return this.nfcPeer;
  },

  defineEventHandlerGetterSetter: function defineEventHandlerGetterSetter(name) {
    Object.defineProperty(this, name, {
      get: function get() {
        return this.__DOM_IMPL__.getEventHandler(name);
      },
      set: function set(handler) {
        this.__DOM_IMPL__.setEventHandler(name, handler);
      }
    });
  },

  eventListenerWasAdded: function(eventType) {
    if (eventType !== "peerready") {
      return;
    }

    let appId = this._window.document.nodePrincipal.appId;
    this._nfcContentHelper.registerTargetForPeerReady(appId);
  },

  eventListenerWasRemoved: function(eventType) {
    if (eventType !== "peerready") {
      return;
    }

    let appId = this._window.document.nodePrincipal.appId;
    this._nfcContentHelper.unregisterTargetForPeerReady(appId);
  },

  notifyTagFound: function notifyTagFound(sessionToken, event, records) {
    if (this.hasDeadWrapper()) {
      dump("this._window or this.__DOM_IMPL__ is a dead wrapper.");
      return;
    }

    if (!this.checkPermissions(["nfc-read", "nfc-write"])) {
      return;
    }

    let tagImpl = new MozNFCTagImpl(this._window, sessionToken, event);
    let tag = this._window.MozNFCTag._create(this._window, tagImpl);
    this.nfcTag = tag;

    let length = records ? records.length : 0;
    let ndefRecords = records ? [] : null;
    for (let i = 0; i < length; i++) {
      let record = records[i];
      ndefRecords.push(new this._window.MozNDEFRecord({tnf: record.tnf,
                                                       type: record.type,
                                                       id: record.id,
                                                       payload: record.payload}));
    }

    let eventData = {
      "tag": tag,
      "ndefRecords": ndefRecords
    };

    debug("fire ontagfound " + sessionToken);
    let tagEvent = new this._window.MozNFCTagEvent("tagfound", eventData);
    this.__DOM_IMPL__.dispatchEvent(tagEvent);
  },

  notifyTagLost: function notifyTagLost(sessionToken) {
    if (this.hasDeadWrapper()) {
      dump("this._window or this.__DOM_IMPL__ is a dead wrapper.");
      return;
    }

    if (!this.checkPermissions(["nfc-read", "nfc-write"])) {
      return;
    }

    if (this.nfcTag && (this.nfcTag.session == sessionToken)) {
      this.nfcTag.isLost = true;
      this.nfcTag = null;
    }

    debug("fire ontaglost " + sessionToken);
    let event = new this._window.Event("taglost");
    this.__DOM_IMPL__.dispatchEvent(event);
  },

  notifyPeerFound: function notifyPeerFound(sessionToken, isPeerReady) {
    if (this.hasDeadWrapper()) {
      dump("this._window or this.__DOM_IMPL__ is a dead wrapper.");
      return;
    }

    if (!this.checkPermissions(["nfc-write"])) {
      return;
    }

    this.session = sessionToken;
    this.nfcPeer = this._createNFCPeer(sessionToken);
    let eventData = { "peer": this.nfcPeer };
    let type = (isPeerReady) ? "peerready" : "peerfound";

    debug("fire on" + type + " " + sessionToken);
    let event = new this._window.MozNFCPeerEvent(type, eventData);
    this.__DOM_IMPL__.dispatchEvent(event);
  },

  notifyPeerLost: function notifyPeerLost(sessionToken) {
    if (this.hasDeadWrapper()) {
      dump("this._window or this.__DOM_IMPL__ is a dead wrapper.");
      return;
    }

    if (!this.checkPermissions(["nfc-write"])) {
      return;
    }

    if (sessionToken != this.session) {
      dump("Unpaired session for notifyPeerLost." + sessionToken);
      return;
    }

    if (this.nfcPeer && (this.nfcPeer.session == sessionToken)) {
      this.nfcPeer.isLost = true;
      this.nfcPeer = null;
    }

    this.session = null;

    debug("fire onpeerlost");
    let event = new this._window.Event("peerlost");
    this.__DOM_IMPL__.dispatchEvent(event);
  },

  checkPermissions: function checkPermissions(perms) {
    let principal = this._window.document.nodePrincipal;
    for (let perm of perms) {
      let permValue =
        Services.perms.testExactPermissionFromPrincipal(principal, perm);
      if (permValue == Ci.nsIPermissionManager.ALLOW_ACTION) {
        return true;
      } else {
        debug("doesn't have " + perm + " permission.");
      }
    }

    return false;
  },

  hasDeadWrapper: function hasDeadWrapper() {
    return Cu.isDeadWrapper(this._window) || Cu.isDeadWrapper(this.__DOM_IMPL__);
  },

  classID: Components.ID("{6ff2b290-2573-11e3-8224-0800200c9a66}"),
  contractID: "@mozilla.org/navigatorNfc;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer,
                                         Ci.nsINfcEventListener]),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([MozNFCTagImpl,
  MozNFCPeerImpl, MozNFCImpl]);
