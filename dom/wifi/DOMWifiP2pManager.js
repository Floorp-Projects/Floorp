/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

const DEBUG = false;

// interface MozWifiP2pGroupOwner implementation.

function MozWifiP2pGroupOwner(aGo) {
  this.groupName = aGo.groupName;
  this.macAddress = aGo.macAddress;
  this.ipAddress = aGo.ipAddress;
  this.passphrase = aGo.passphrase;
  this.ssid = aGo.ssid;
  this.wpsCapabilities = aGo.wpsCapabilities;
  this.freq = aGo.freq;
  this.isLocal = aGo.isLocal;
}

MozWifiP2pGroupOwner.prototype = {
  classID: Components.ID("{a9b81450-349d-11e3-aa6e-0800200c9a66}"),
  contractID: "@mozilla.org/wifip2pgroupowner;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports])
};

// interface MozWifiP2pManager implementation.

const MOZ_WIFIP2PMANAGER_CONTRACTID = "@mozilla.org/wifip2pmanager;1";
const MOZ_WIFIP2PMANAGER_CID        = Components.ID("{8d9125a0-3498-11e3-aa6e-0800200c9a66}");

function MozWifiP2pManager() {
  this.defineEventHandlerGetterSetter("onstatuschange");
  this.defineEventHandlerGetterSetter("onpeerinfoupdate");
  this.defineEventHandlerGetterSetter("onenabled");
  this.defineEventHandlerGetterSetter("ondisabled");

  this.currentPeer = null;
  this.enabled = false;
  this.groupOwner = null;
}

// For smaller, read-only APIs, we expose any property that doesn't begin with
// an underscore.
function exposeReadOnly(obj) {
  let exposedProps = {};
  for (let i in obj) {
    if (i[0] === "_") {
      continue;
    }
    exposedProps[i] = "r";
  }

  obj.__exposedProps__ = exposedProps;
  return obj;
}

function debug(msg) {
  if (DEBUG) {
    dump('-------------- MozWifiP2pManager: ' + msg);
  }
}

MozWifiP2pManager.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  classID:        MOZ_WIFIP2PMANAGER_CID,
  contractID:     MOZ_WIFIP2PMANAGER_CONTRACTID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver,
                                         Ci.nsISupports]),

  //
  // nsIDOMGlobalPropertyInitializer implementation.
  //

  init: function(aWindow) {
    const messages = ["WifiP2pManager:setScanEnabled:Return:OK",
                      "WifiP2pManager:setScanEnabled:Return:NO",
                      "WifiP2pManager:getPeerList:Return:OK",
                      "WifiP2pManager:getPeerList:Return:NO",
                      "WifiP2pManager:connect:Return:OK",
                      "WifiP2pManager:connect:Return:NO",
                      "WifiP2pManager:disconnect:Return:OK",
                      "WifiP2pManager:disconnect:Return:NO",
                      "WifiP2pManager:setPairingConfirmation:Return",
                      "WifiP2pManager:setDeviceName:Return:OK",
                      "WifiP2pManager:setDeviceName:Return:NO",

                      "WifiP2pManager:p2pDown",
                      "WifiP2pManager:p2pUp",
                      "WifiP2pManager:onconnecting",
                      "WifiP2pManager:onconnected",
                      "WifiP2pManager:ondisconnected",
                      "WifiP2pManager:ongroupnstop",
                      "WifiP2pManager:onconnectingfailed",
                      "WifiP2pManager:onwpstimeout",
                      "WifiP2pManager:onwpsfail",
                      "WifiP2pManager:onpeerinfoupdate",
                      ];

    this.initDOMRequestHelper(aWindow, messages);
    this._mm = Cc["@mozilla.org/childprocessmessagemanager;1"].getService(Ci.nsISyncMessageSender);

    // Notify the internal a new DOM mananger is created.
    let state = this._mm.sendSyncMessage("WifiP2pManager:getState")[0];
    if (state) {
      debug('State: ' + JSON.stringify(state));
    } else {
      debug('Failed to get state');
    }
  },

  uninit: function() {
  },

  _sendMessageForRequest: function(name, data, request) {
    let id = this.getRequestId(request);
    this._mm.sendAsyncMessage(name, { data: data, rid: id, mid: this._id });
  },

  receiveMessage: function(aMessage) {
    let msg = aMessage.json;
    if (msg.mid && msg.mid !== this._id) {
      return;
    }

    let request;
    switch (aMessage.name) {
     case "WifiP2pManager:setScanEnabled:Return:OK":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireSuccess(request, exposeReadOnly(msg.data));
        break;

      case "WifiP2pManager:setScanEnabled:Return:NO":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireError(request, "Unable to enable/disable Wifi P2P peer discovery.");
        break;

     case "WifiP2pManager:getPeerList:Return:OK":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireSuccess(request, msg.data);
        break;

      case "WifiP2pManager:getPeerList:Return:NO":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireError(request, "Unable to disable Wifi P2P peer discovery.");
        break;

      case "WifiP2pManager:connect:Return:OK":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireSuccess(request, exposeReadOnly(msg.data));
        break;

      case "WifiP2pManager:connect:Return:NO":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireError(request, "Unable to connect to Wifi P2P peer.");
        break;

      case "WifiP2pManager:disconnect:Return:OK":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireSuccess(request, exposeReadOnly(msg.data));
        break;

      case "WifiP2pManager:disconnect:Return:NO":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireError(request, "Unable to disconnect to Wifi P2P peer.");
        break;

      case "WifiP2pManager:setDeviceName:Return:OK":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireSuccess(request, exposeReadOnly(msg.data));
        break;

      case "WifiP2pManager:setDeviceName:Return:NO":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireError(request, "Unable to set device name.");
        break;

      case "WifiP2pManager:p2pDown":
        this.enabled = false;
        this.currentPeer = null;
        this._fireEnabledOrDisabled(false);
        break;

      case "WifiP2pManager:p2pUp":
        this.enabled = true;
        this._fireEnabledOrDisabled(true);
        break;

      case "WifiP2pManager:onconnecting":
        debug('onconnecting with peer: ' + JSON.stringify(msg.peer));
        this.currentPeer = msg.peer;
        this._fireStatusChangeEvent(msg.peer.address);
        break;

      case "WifiP2pManager:onconnected":
        debug('onconnected with peer: ' + JSON.stringify(msg.peer));
        this.currentPeer = msg.peer;
        this.groupOwner = new MozWifiP2pGroupOwner(msg.groupOwner);
        this._fireStatusChangeEvent(msg.peer.address);
        break;

      case "WifiP2pManager:ondisconnected":
        debug('ondisconnected with peer: ' + JSON.stringify(msg.peer));
        this.currentPeer = null;
        this.groupOwner = null;
        this._fireStatusChangeEvent(msg.peer.address);
        break;

      case "WifiP2pManager:onconnectingfailed":
        this._fireStatusChangeEvent(null);
        break;

      case "WifiP2pManager:onwpstimeout":
        this._fireStatusChangeEvent(null);
        break;

      case "WifiP2pManager:onwpsfail":
        this._fireStatusChangeEvent(null);
        break;

      case "WifiP2pManager:onpeerinfoupdate":
        this._firePeerInfoUpdateEvent();
        break;
    }
  },

  _firePeerInfoUpdateEvent: function PeerInfoUpdate() {
    let evt = new this._window.Event("peerinfoupdate");
    this.__DOM_IMPL__.dispatchEvent(evt);
  },

  _fireStatusChangeEvent: function WifiP2pStatusChange(peerAddress) {
    let evt = new this._window.MozWifiP2pStatusChangeEvent("statuschange",
                                                           { peerAddress: peerAddress });
    this.__DOM_IMPL__.dispatchEvent(evt);
  },

  _fireEnabledOrDisabled: function enabledDisabled(enabled) {
    let evt = new this._window.Event(enabled ? "enabled" : "disabled");
    this.__DOM_IMPL__.dispatchEvent(evt);
  },

  //
  // WifiP2pManager.webidl implementation.
  //

  enableScan: function () {
    let request = this.createRequest();
    this._sendMessageForRequest("WifiP2pManager:enableScan", null, request);
    return request;
  },

  disableScan: function () {
    let request = this.createRequest();
    this._sendMessageForRequest("WifiP2pManager:disableScan", null, request);
    return request;
  },

  setScanEnabled: function(enabled) {
    let request = this.createRequest();
    this._sendMessageForRequest("WifiP2pManager:setScanEnabled", enabled, request);
    return request;
  },

  connect: function (address, wpsMethod, goIntent) {
    let request = this.createRequest();
    let connectionInfo = { address: address, wpsMethod: wpsMethod, goIntent: goIntent };
    this._sendMessageForRequest("WifiP2pManager:connect", connectionInfo, request);
    return request;
  },

  disconnect: function (address) {
    let request = this.createRequest();
    this._sendMessageForRequest("WifiP2pManager:disconnect", address, request);
    return request;
  },

  getPeerList: function () {
    let request = this.createRequest();
    this._sendMessageForRequest("WifiP2pManager:getPeerList", null, request);
    return request;
  },

  setPairingConfirmation: function (accepted, pin) {
    let request = this.createRequest();
    let result = { accepted: accepted, pin: pin };
    this._sendMessageForRequest("WifiP2pManager:setPairingConfirmation", result, request);
    return request;
  },

  setDeviceName: function(newDeviceName) {
    let request = this.createRequest();
    this._sendMessageForRequest("WifiP2pManager:setDeviceName", newDeviceName, request);
    return request;
  },

  // Helpers.
  defineEventHandlerGetterSetter: function(event) {
    Object.defineProperty(this, event, {
      get: function() {
        return this.__DOM_IMPL__.getEventHandler(event);
      },

      set: function(handler) {
        this.__DOM_IMPL__.setEventHandler(event, handler);
      }
    });
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([MozWifiP2pManager, MozWifiP2pGroupOwner]);
