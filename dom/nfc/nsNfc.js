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
    this.promise = this.createPromiseWithId((aResolverId) => {
      this._requestId = btoa(aResolverId);
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

    let records = new this._window.Array();
    for (let i = 0; i < aRecords.length; i++) {
      let record = aRecords[i];
      records.push(new this._window.MozNDEFRecord({tnf: record.tnf,
                                                   type: record.type,
                                                   id: record.id,
                                                   payload: record.payload}));
    }
    resolver.resolve(records);
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
var TagType = {
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

  createTech: { "ISO-DEP": (win, tag) => { return new win.MozIsoDepTech(tag); },
                "NFC-A"  : (win, tag) => { return new win.MozNfcATech(tag);   },
              },

  // NFCTag interface:
  readNDEF: function readNDEF() {
    if (this.isLost) {
      throw new this._window.Error("NFCTag object is invalid");
    }

    let callback = new NfcCallback(this._window);
    this._nfcContentHelper.readNDEF(this.session, callback);
    return callback.promise;
  },

  writeNDEF: function writeNDEF(records) {
    if (this.isLost) {
      throw new this._window.Error("NFCTag object is invalid");
    }

    if (this.isReadOnly) {
      throw new this._window.Error("NFCTag object is read-only");
    }

    let ndefLen = 0;
    for (let record of records) {
      ndefLen += record.size;
    }

    if (ndefLen > this.maxNDEFSize) {
      throw new this._window.Error("Exceed max NDEF size");
    }

    let callback = new NfcCallback(this._window);
    this._nfcContentHelper.writeNDEF(records, false, this.session, callback);
    return callback.promise;
  },

  makeReadOnly: function makeReadOnly() {
    if (this.isLost) {
      throw new this._window.Error("NFCTag object is invalid");
    }

    if (!this.canBeMadeReadOnly) {
      throw new this._window.Error("NFCTag object cannot be made read-only");
    }

    let callback = new NfcCallback(this._window);
    this._nfcContentHelper.makeReadOnly(this.session, callback);
    return callback.promise;
  },

  format: function format() {
    if (this.isLost) {
      throw new this._window.Error("NFCTag object is invalid");
    }

    if (!this.isFormatable) {
      throw new this._window.Error("NFCTag object is not formatable");
    }

    let callback = new NfcCallback(this._window);
    this._nfcContentHelper.format(this.session, callback);
    return callback.promise;
  },

  selectTech: function selectTech(tech) {
    if (this.isLost) {
      throw new this._window.Error("NFCTag object is invalid");
    }

    if (this.techList.indexOf(tech) == -1) {
      throw new this._window.Error(
        "NFCTag does not contain selected tag technology");
    }

    if (this.createTech[tech] === undefined) {
      throw new this._window.Error("Technology is not supported now");
    }

    return this.createTech[tech](this._window, this._contentObj);
  },

  transceive: function transceive(tech, cmd) {
    if (this.isLost) {
      throw new this._window.Error("NFCTag object is invalid");
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
      throw new this._window.Error("NFCPeer object is invalid");
    }

    // Just forward sendNDEF to writeNDEF
    let callback = new NfcCallback(this._window);
    this._nfcContentHelper.writeNDEF(records, true, this.session, callback);
    return callback.promise;
  },

  sendFile: function sendFile(blob) {
    if (this.isLost) {
      throw new this._window.Error("NFCPeer object is invalid");
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
var RFState = {
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
}
MozNFCImpl.prototype = {
  _nfcContentHelper: null,
  window: null,
  _tabId: null,
  _innerWindowId: null,
  _rfState: null,
  _contentObj: null,
  nfcPeer: null,
  nfcTag: null,
  eventService: null,

  init: function init(aWindow) {
    debug("MozNFCImpl init called");
    this.window = aWindow;
    let util = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIDOMWindowUtils);
    this._innerWindowId = util.currentInnerWindowID;

    this.defineEventHandlerGetterSetter("ontagfound");
    this.defineEventHandlerGetterSetter("ontaglost");
    this.defineEventHandlerGetterSetter("onpeerready");
    this.defineEventHandlerGetterSetter("onpeerfound");
    this.defineEventHandlerGetterSetter("onpeerlost");

    Services.obs.addObserver(this, "inner-window-destroyed",
                             /* weak-ref */ false);

    if (this._nfcContentHelper) {
      this._tabId = this.getTabId(aWindow);
      this._nfcContentHelper.addEventListener(this, this._tabId);
      this._rfState = this._nfcContentHelper.queryRFState();
    }
  },

  getTabId: function getTabId(aWindow) {
    let tabId;
    // For now, we assume app will run in oop mode so we can get
    // tab id for each app. Fix bug 1116449 if we are going to
    // support in-process mode.
    let docShell = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIWebNavigation)
                          .QueryInterface(Ci.nsIDocShell);
    try {
      tabId = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsITabChild)
                      .tabId;
    } catch(e) {
      // Only parent process does not have tab id, so in this case
      // NfcContentHelper is used by system app. Use -1(tabId) to
      // indicate its system app.
      let inParent = Cc["@mozilla.org/xre/app-info;1"]
                       .getService(Ci.nsIXULRuntime)
                       .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
      if (inParent) {
        tabId = Ci.nsINfcBrowserAPI.SYSTEM_APP_ID;
      } else {
        throw Components.Exception("Can't get tab id in child process",
                                   Cr.NS_ERROR_UNEXPECTED);
      }
    }

    return tabId;
  },

  // Only apps which have nfc-manager permission can call the following interfaces
  // 'checkP2PRegistration' , 'notifyUserAcceptedP2P' , 'notifySendFileStatus',
  // 'startPoll', 'stopPoll', and 'powerOff'.
  checkP2PRegistration: function checkP2PRegistration(manifestUrl) {
    // Get the AppID and pass it to ContentHelper
    let appID = appsService.getAppLocalIdByManifestURL(manifestUrl);

    let callback = new NfcCallback(this.window);
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
    let callback = new NfcCallback(this.window);
    this._nfcContentHelper.changeRFState(RFState.DISCOVERY, callback);
    return callback.promise;
  },

  stopPoll: function stopPoll() {
    let callback = new NfcCallback(this.window);
    this._nfcContentHelper.changeRFState(RFState.LISTEN, callback);
    return callback.promise;
  },

  powerOff: function powerOff() {
    let callback = new NfcCallback(this.window);
    this._nfcContentHelper.changeRFState(RFState.IDLE, callback);
    return callback.promise;
  },

  get enabled() {
    return this._rfState != RFState.IDLE;
  },

  observe: function observe(subject, topic, data) {
    if (topic !== "inner-window-destroyed") {
      return;
    }

    let wId = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
    if (wId != this._innerWindowId) {
      return;
    }

    this._nfcContentHelper.removeEventListener(this._tabId);
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

    let appId = this.window.document.nodePrincipal.appId;
    this._nfcContentHelper.registerTargetForPeerReady(appId);
  },

  eventListenerWasRemoved: function(eventType) {
    if (eventType !== "peerready") {
      return;
    }

    let appId = this.window.document.nodePrincipal.appId;
    this._nfcContentHelper.unregisterTargetForPeerReady(appId);
  },

  notifyTagFound: function notifyTagFound(sessionToken, tagInfo, ndefInfo, records) {
    if (!this.handleTagFound(sessionToken, tagInfo, ndefInfo, records)) {
      this._nfcContentHelper.callDefaultFoundHandler(sessionToken, false, records);
    };
  },

  /**
   * Handles Tag Found event.
   *
   * returns true if the app could process this event, false otherwise.
   */
  handleTagFound: function handleTagFound(sessionToken, tagInfo, ndefInfo, records) {
    if (this.hasDeadWrapper()) {
      dump("this.window or this.__DOM_IMPL__ is a dead wrapper.");
      return false;
    }

    if (!this.eventService.hasListenersFor(this.__DOM_IMPL__, "tagfound")) {
      debug("ontagfound is not registered.");
      return false;
    }

    if (!this.checkPermissions(["nfc"])) {
      return false;
    }

    let tagImpl = new MozNFCTagImpl(this.window, sessionToken, tagInfo, ndefInfo);
    let tag = this.window.MozNFCTag._create(this.window, tagImpl);

    tagImpl._contentObj = tag;
    this.nfcTag = tag;

    let length = records ? records.length : 0;
    let ndefRecords = records ? [] : null;
    for (let i = 0; i < length; i++) {
      let record = records[i];
      ndefRecords.push(new this.window.MozNDEFRecord({tnf: record.tnf,
                                                       type: record.type,
                                                       id: record.id,
                                                       payload: record.payload}));
    }

    let eventData = {
      "cancelable": true,
      "tag": tag,
      "ndefRecords": ndefRecords
    };

    debug("fire ontagfound " + sessionToken);
    let tagEvent = new this.window.MozNFCTagEvent("tagfound", eventData);
    this.__DOM_IMPL__.dispatchEvent(tagEvent);

    // If defaultPrevented is false, means we need to take the default action
    // for this event - redirect this event to System app. Before redirecting to
    // System app, we need revoke the tag object first.
    if (!tagEvent.defaultPrevented) {
      this.notifyTagLost(sessionToken);
    }

    return tagEvent.defaultPrevented;
  },

  notifyTagLost: function notifyTagLost(sessionToken) {
    if (!this.handleTagLost(sessionToken)) {
      this._nfcContentHelper.callDefaultLostHandler(sessionToken, false);
    }
  },

  handleTagLost: function handleTagLost(sessionToken) {
    if (this.hasDeadWrapper()) {
      dump("this.window or this.__DOM_IMPL__ is a dead wrapper.");
      return false;
    }

    if (!this.checkPermissions(["nfc"])) {
      return false;
    }

    if (!this.nfcTag) {
      debug("No NFCTag object existing.");
      return false;
    }

    this.nfcTag.notifyLost();
    this.nfcTag = null;

    debug("fire ontaglost " + sessionToken);
    let event = new this.window.Event("taglost");
    this.__DOM_IMPL__.dispatchEvent(event);

    return true;
  },

  notifyPeerFound: function notifyPeerFound(sessionToken, isPeerReady) {
    if (!this.handlePeerFound(sessionToken, isPeerReady)) {
      this._nfcContentHelper.callDefaultFoundHandler(sessionToken, true, null);
    }
  },

  /**
   * Handles Peer Found/Peer Ready event.
   *
   * returns true if the app could process this event, false otherwise.
   */
  handlePeerFound: function handlePeerFound(sessionToken, isPeerReady) {
    if (this.hasDeadWrapper()) {
      dump("this.window or this.__DOM_IMPL__ is a dead wrapper.");
      return false;
    }

    if (!isPeerReady &&
        !this.eventService.hasListenersFor(this.__DOM_IMPL__, "peerfound")) {
      debug("onpeerfound is not registered.");
      return false;
    }

    let perm = isPeerReady ? ["nfc-share"] : ["nfc"];
    if (!this.checkPermissions(perm)) {
      return false;
    }

    let peerImpl = new MozNFCPeerImpl(this.window, sessionToken);
    this.nfcPeer = this.window.MozNFCPeer._create(this.window, peerImpl);

    let eventType;
    let eventData = {
      "peer": this.nfcPeer
    };

    if (isPeerReady) {
      eventType = "peerready";
    } else {
      eventData.cancelable = true;
      eventType = "peerfound";
    }

    debug("fire on" + eventType + " " + sessionToken);
    let event = new this.window.MozNFCPeerEvent(eventType, eventData);
    this.__DOM_IMPL__.dispatchEvent(event);

    // For peerready we don't take the default action.
    if (isPeerReady) {
      return true;
    }

    // If defaultPrevented is false, means we need to take the default action
    // for this event - redirect this event to System app. Before redirecting to
    // System app, we need revoke the peer object first.
    if (!event.defaultPrevented) {
      this.notifyPeerLost(sessionToken);
    }

    return event.defaultPrevented;
  },

  notifyPeerLost: function notifyPeerLost(sessionToken) {
    if (!this.handlePeerLost(sessionToken)) {
      this._nfcContentHelper.callDefaultLostHandler(sessionToken, true);
    }
  },

  handlePeerLost: function handlePeerLost(sessionToken) {
    if (this.hasDeadWrapper()) {
      dump("this.window or this.__DOM_IMPL__ is a dead wrapper.");
      return false;
    }

    if (!this.checkPermissions(["nfc", "nfc-share"])) {
      return false;
    }

    if (!this.nfcPeer) {
      debug("No NFCPeer object existing.");
      return false;
    }

    this.nfcPeer.notifyLost();
    this.nfcPeer = null;

    debug("fire onpeerlost");
    let event = new this.window.Event("peerlost");
    this.__DOM_IMPL__.dispatchEvent(event);

    return true;
  },

  notifyRFStateChanged: function notifyRFStateChanged(rfState) {
    this._rfState = rfState;
  },

  notifyFocusChanged: function notifyFocusChanged(focus) {
    if (focus) {
      return;
    }

    if (this.nfcTag) {
      debug("losing focus, call taglost.");
      this.notifyTagLost(this.nfcTag.session);
    }

    if (this.nfcPeer) {
      debug("losing focus, call peerlost.");
      this.notifyPeerLost(this.nfcPeer.session);
    }
  },

  checkPermissions: function checkPermissions(perms) {
    let principal = this.window.document.nodePrincipal;
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
    return Cu.isDeadWrapper(this.window) || Cu.isDeadWrapper(this.__DOM_IMPL__);
  },

  classID: Components.ID("{6ff2b290-2573-11e3-8224-0800200c9a66}"),
  contractID: "@mozilla.org/nfc/manager;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer,
                                         Ci.nsINfcEventListener,
                                         Ci.nsIObserver]),
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
