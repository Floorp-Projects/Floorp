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
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

XPCOMUtils.defineLazyServiceGetter(this,
                                   "appsService",
                                   "@mozilla.org/AppsService;1",
                                   "nsIAppsService");

function NfcCallback(aWindow) {
  this._window = aWindow;
  this.initDOMRequestHelper(aWindow, null);
  this._createPromise();
}
NfcCallback.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  _window: null,
  promise: null,
  _requestId: null,

  _createPromise: function _createPromise() {
    this.promise = this.createPromise((aResolve, aReject) => {
      this._requestId = btoa(this.getPromiseResolverId({
        resolve: aResolve,
        reject: aReject
      }));
    });
  },

  getCallbackId: function getCallbackId() {
    return this._requestId;
  },

  notifySuccess: function notifySuccess() {
    let resolver = this.takePromiseResolver(atob(this._requestId));
    if (!resolver) {
      debug("can not find promise resolver for id: " + this._requestId);
      return;
    }
    resolver.resolve();
  },

  notifySuccessWithBoolean: function notifySuccessWithBoolean(aResult) {
    let resolver = this.takePromiseResolver(atob(this._requestId));
    if (!resolver) {
      debug("can not find promise resolver for id: " + this._requestId);
      return;
    }
    resolver.resolve(aResult);
  },

  notifySuccessWithNDEFRecords: function notifySuccessWithNDEFRecords(aRecords) {
    let resolver = this.takePromiseResolver(atob(this._requestId));
    if (!resolver) {
      debug("can not find promise resolver for id: " + this._requestId);
      return;
    }
    resolver.resolve(aRecords);
  },

  notifySuccessWithByteArray: function notifySuccessWithByteArray(aArray) {
    let resolver = this.takePromiseResolver(atob(this._requestId));
    if (!resolver) {
      debug("can not find promise resolver for id: " + this._requestId);
      return;
    }
    resolver.resolve(Cu.cloneInto(aArray, this._window));
  },

  notifyError: function notifyError(aErrorMsg) {
    let resolver = this.takePromiseResolver(atob(this._requestId));
    if (!resolver) {
      debug("can not find promise resolver for id: " + this._requestId +
           ", errormsg: " + aErrorMsg);
      return;
    }
    resolver.reject(new this._window.Error(aErrorMsg));
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver,
                                         Ci.nsINfcRequestCallback]),
};

// Should be mapped to the NFCTagType defined in MozNFCTag.webidl.
let TagType = {
  TYPE1: "Type1",
  TYPE2: "Type2",
  TYPE3: "Type3",
  TYPE4: "Type4",
  MIFARE_CLASSIC: "MIFARE-Classic"
};

/**
 * Implementation of NFCTag.
 *
 * @param window        global window object.
 * @param sessionToken  session token received from parent process.
 * @param tagInfo       type of nsITagInfo received from parent process.
 * @parem ndefInfo      type of nsITagNDEFInfo received from parent process.
 */
function MozNFCTagImpl(window, sessionToken, tagInfo, ndefInfo) {
  debug("In MozNFCTagImpl Constructor");
  this._nfcContentHelper = Cc["@mozilla.org/nfc/content-helper;1"]
                             .getService(Ci.nsINfcContentHelper);
  this._window = window;
  this.session = sessionToken;
  this.techList = tagInfo.techList;
  this.id = Cu.cloneInto(tagInfo.tagId, window);

  if (ndefInfo) {
    this.type = ndefInfo.tagType;
    this.maxNDEFSize = ndefInfo.maxNDEFSize;
    this.isReadOnly = ndefInfo.isReadOnly;
    this.isFormatable = ndefInfo.isFormatable;
    this.canBeMadeReadOnly = this.type == TagType.TYPE1 ||
                             this.type == TagType.TYPE2 ||
                             this.type == TagType.MIFARE_CLASSIC;
  }
}
MozNFCTagImpl.prototype = {
  _nfcContentHelper: null,
  _window: null,
  session: null,
  techList: null,
  id: null,
  type: null,
  maxNDEFSize: null,
  isReadOnly: null,
  isFormatable: null,
  canBeMadeReadOnly: null,
  isLost: false,

  createTech: { "ISO-DEP": (win, tag) => { return new win.MozIsoDepTech(tag); }
              },

  // NFCTag interface:
  readNDEF: function readNDEF() {
    if (this.isLost) {
      throw new this._window.DOMError("InvalidStateError", "NFCTag object is invalid");
    }

    let callback = new NfcCallback(this._window);
    this._nfcContentHelper.readNDEF(this.session, callback);
    return callback.promise;
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

    let callback = new NfcCallback(this._window);
    this._nfcContentHelper.writeNDEF(records, this.session, callback);
    return callback.promise;
  },

  makeReadOnly: function makeReadOnly() {
    if (this.isLost) {
      throw new this._window.DOMError("InvalidStateError", "NFCTag object is invalid");
    }

    if (!this.canBeMadeReadOnly) {
      throw new this._window.DOMError("InvalidAccessError",
                                      "NFCTag object cannot be made read-only");
    }

    let callback = new NfcCallback(this._window);
    this._nfcContentHelper.makeReadOnly(this.session, callback);
    return callback.promise;
  },

  format: function format() {
    if (this.isLost) {
      throw new this._window.DOMError("InvalidStateError", "NFCTag object is invalid");
    }

    if (!this.isFormatable) {
      throw new this._window.DOMError("InvalidAccessError",
                                      "NFCTag object is not formatable");
    }

    let callback = new NfcCallback(this._window);
    this._nfcContentHelper.format(this.session, callback);
    return callback.promise;
  },

  selectTech: function selectTech(tech) {
    if (this.isLost) {
      throw new this._window.DOMError("InvalidStateError", "NFCTag object is invalid");
    }

    if (this.techList.indexOf(tech) == -1) {
      throw new this._window.DOMError("InvalidAccessError",
        "NFCTag does not contain selected tag technology");
    }

    if (this.createTech[tech] === undefined) {
      throw new this._window.DOMError("InvalidAccessError",
        "Technology is not supported now");
    }

    return this.createTech[tech](this._window, this._contentObj);
  },

  transceive: function transceive(tech, cmd) {
    if (this.isLost) {
      throw new this._window.DOMError("InvalidStateError", "NFCTag object is invalid");
    }

    let callback = new NfcCallback(this._window);
    this._nfcContentHelper.transceive(this.session, tech, cmd, callback);
    return callback.promise;
  },

  notifyLost: function notifyLost() {
    this.isLost = true;
  },

  classID: Components.ID("{4e1e2e90-3137-11e3-aa6e-0800200c9a66}"),
  contractID: "@mozilla.org/nfc/tag;1",
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
    let callback = new NfcCallback(this._window);
    this._nfcContentHelper.writeNDEF(records, this.session, callback);
    return callback.promise;
  },

  sendFile: function sendFile(blob) {
    if (this.isLost) {
      throw new this._window.DOMError("InvalidStateError", "NFCPeer object is invalid");
    }

    let data = {
      "blob": blob
    };

    let callback = new NfcCallback(this._window);
    this._nfcContentHelper.sendFile(Cu.cloneInto(data, this._window),
                                    this.session, callback);
    return callback.promise;
  },

  notifyLost: function notifyLost() {
    this.isLost = true;
  },

  classID: Components.ID("{c1b2bcf0-35eb-11e3-aa6e-0800200c9a66}"),
  contractID: "@mozilla.org/nfc/peer;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer]),
};

// Should be mapped to the RFState defined in WebIDL.
let RFState = {
  IDLE: "idle",
  LISTEN: "listen",
  DISCOVERY: "discovery"
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
    debug("No NFC support.");
  }

  this.eventService = Cc["@mozilla.org/eventlistenerservice;1"]
                        .getService(Ci.nsIEventListenerService);
  this._nfcContentHelper.addEventListener(this);
}
MozNFCImpl.prototype = {
  _nfcContentHelper: null,
  _window: null,
  _rfState: null,
  _contentObj: null,
  nfcPeer: null,
  nfcTag: null,
  eventService: null,

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
      this._rfState = this._nfcContentHelper.queryRFState();
    }
  },

  // Only apps which have nfc-manager permission can call the following interfaces
  // 'checkP2PRegistration' , 'notifyUserAcceptedP2P' , 'notifySendFileStatus',
  // 'startPoll', 'stopPoll', and 'powerOff'.
  checkP2PRegistration: function checkP2PRegistration(manifestUrl) {
    // Get the AppID and pass it to ContentHelper
    let appID = appsService.getAppLocalIdByManifestURL(manifestUrl);

    let callback = new NfcCallback(this._window);
    this._nfcContentHelper.checkP2PRegistration(appID, callback);
    return callback.promise;
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
    let callback = new NfcCallback(this._window);
    this._nfcContentHelper.changeRFState(RFState.DISCOVERY, callback);
    return callback.promise;
  },

  stopPoll: function stopPoll() {
    let callback = new NfcCallback(this._window);
    this._nfcContentHelper.changeRFState(RFState.LISTEN, callback);
    return callback.promise;
  },

  powerOff: function powerOff() {
    let callback = new NfcCallback(this._window);
    this._nfcContentHelper.changeRFState(RFState.IDLE, callback);
    return callback.promise;
  },

  get enabled() {
    return this._rfState != RFState.IDLE;
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

  notifyTagFound: function notifyTagFound(sessionToken, tagInfo, ndefInfo, records) {
    if (this.hasDeadWrapper()) {
      dump("this._window or this.__DOM_IMPL__ is a dead wrapper.");
      return;
    }

    if (!this.eventService.hasListenersFor(this.__DOM_IMPL__, "tagfound")) {
      debug("ontagfound is not registered.");
      return;
    }

    if (!this.checkPermissions(["nfc"])) {
      return;
    }

    this.eventService.addSystemEventListener(this._window, "visibilitychange",
      this, /* useCapture */false);

    let tagImpl = new MozNFCTagImpl(this._window, sessionToken, tagInfo, ndefInfo);
    let tag = this._window.MozNFCTag._create(this._window, tagImpl);

    tagImpl._contentObj = tag;
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

    if (!this.checkPermissions(["nfc"])) {
      return;
    }

    if (!this.nfcTag) {
      debug("No NFCTag object existing.");
      return;
    }

    // Remove system event listener only when tag and peer are both lost.
    if (!this.nfcPeer) {
      this.eventService.removeSystemEventListener(this._window, "visibilitychange",
        this, /* useCapture */false);
    }

    this.nfcTag.notifyLost();
    this.nfcTag = null;

    debug("fire ontaglost " + sessionToken);
    let event = new this._window.Event("taglost");
    this.__DOM_IMPL__.dispatchEvent(event);
  },

  notifyPeerFound: function notifyPeerFound(sessionToken, isPeerReady) {
    if (this.hasDeadWrapper()) {
      dump("this._window or this.__DOM_IMPL__ is a dead wrapper.");
      return;
    }

    if (!isPeerReady &&
        !this.eventService.hasListenersFor(this.__DOM_IMPL__, "peerfound")) {
      debug("onpeerfound is not registered.");
      return;
    }

    let perm = isPeerReady ? ["nfc-share"] : ["nfc"];
    if (!this.checkPermissions(perm)) {
      return;
    }

    this.eventService.addSystemEventListener(this._window, "visibilitychange",
      this, /* useCapture */false);

    let peerImpl = new MozNFCPeerImpl(this._window, sessionToken);
    this.nfcPeer = this._window.MozNFCPeer._create(this._window, peerImpl);
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

    if (!this.checkPermissions(["nfc", "nfc-share"])) {
      return;
    }

    if (!this.nfcPeer) {
      debug("No NFCPeer object existing.");
      return;
    }

    // Remove system event listener only when tag and peer are both lost.
    if (!this.nfcTag) {
      this.eventService.removeSystemEventListener(this._window, "visibilitychange",
        this, /* useCapture */false);
    }

    this.nfcPeer.notifyLost();
    this.nfcPeer = null;

    debug("fire onpeerlost");
    let event = new this._window.Event("peerlost");
    this.__DOM_IMPL__.dispatchEvent(event);
  },

  handleEvent: function handleEvent (event) {
    if (!this._window.document.hidden) {
      return;
    }

    if (this.nfcTag) {
      debug("handleEvent notifyTagLost");
      this.notifyTagLost(this.nfcTag.session);
    }

    if (this.nfcPeer) {
      debug("handleEvent notifyPeerLost");
      this.notifyPeerLost(this.nfcPeer.session);
    }
  },

  notifyRFStateChange: function notifyRFStateChange(rfState) {
    this._rfState = rfState;
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
  contractID: "@mozilla.org/nfc/manager;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer,
                                         Ci.nsINfcEventListener,
                                         Ci.nsIDOMEventListener]),
};

function NFCSendFileWrapper() {
}
NFCSendFileWrapper.prototype = {
  // nsISystemMessagesWrapper implementation.
  wrapMessage: function wrapMessage(aMessage, aWindow) {
    let peerImpl = new MozNFCPeerImpl(aWindow, aMessage.sessionToken);
    let peer = aWindow.MozNFCPeer._create(aWindow, peerImpl);

    delete aMessage.sessionToken;
    aMessage = Cu.cloneInto(aMessage, aWindow);
    aMessage.peer = peer;
    return aMessage;
  },

  classDescription: "NFCSendFileWrapper",
  classID: Components.ID("{c5063a5c-8cb9-41d2-baf5-56062a2e30e9}"),
  contractID: "@mozilla.org/dom/system-messages/wrapper/nfc-manager-send-file;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISystemMessagesWrapper])
};

function NFCTechDiscoveredWrapper() {
}
NFCTechDiscoveredWrapper.prototype = {
  // nsISystemMessagesWrapper implementation.
  wrapMessage: function wrapMessage(aMessage, aWindow) {
    aMessage = Cu.cloneInto(aMessage, aWindow);
    if (aMessage.isP2P) {
      let peerImpl = new MozNFCPeerImpl(aWindow, aMessage.sessionToken);
      let peer = aWindow.MozNFCPeer._create(aWindow, peerImpl);
      aMessage.peer = peer;
    }

    delete aMessage.isP2P;
    delete aMessage.sessionToken;

    return aMessage;
  },

  classDescription: "NFCTechDiscoveredWrapper",
  classID: Components.ID("{2e7f9285-3c72-4e1f-b985-141a00a23a75}"),
  contractID: "@mozilla.org/dom/system-messages/wrapper/nfc-manager-tech-discovered;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISystemMessagesWrapper])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([MozNFCTagImpl,
  MozNFCPeerImpl, MozNFCImpl, NFCSendFileWrapper, NFCTechDiscoveredWrapper]);
