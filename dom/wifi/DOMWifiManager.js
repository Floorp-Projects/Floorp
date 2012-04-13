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

const DEBUG = false; // set to false to suppress debug messages

const DOMWIFIMANAGER_CONTRACTID = "@mozilla.org/wifimanager;1";
const DOMWIFIMANAGER_CID        = Components.ID("{2cf775a7-1837-410c-9e26-323c42e076da}");

function DOMWifiManager() {
}

DOMWifiManager.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  classID:   DOMWIFIMANAGER_CID,
  classInfo: XPCOMUtils.generateCI({classID: DOMWIFIMANAGER_CID,
                                    contractID: DOMWIFIMANAGER_CONTRACTID,
                                    classDescription: "DOMWifiManager",
                                    interfaces: [Ci.nsIDOMWifiManager],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT}),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMWifiManager,
                                         Ci.nsIDOMGlobalPropertyInitializer]),

  // nsIDOMGlobalPropertyInitializer implementation
  init: function(aWindow) {
    let principal = aWindow.document.nodePrincipal;
    let secMan = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(Ci.nsIScriptSecurityManager);

    let perm = (principal == secMan.getSystemPrincipal()) ?
                 Ci.nsIPermissionManager.ALLOW_ACTION :
                 Services.perms.testExactPermission(principal.URI, "wifi-manage");

    // Only pages with perm set can use the wifi manager.
    this._hasPrivileges = perm == Ci.nsIPermissionManager.ALLOW_ACTION;

    // Maintain this state for synchronous APIs.
    this._currentNetwork = null;
    this._connectionStatus = "disconnected";
    this._enabled = true;
    this._lastConnectionInfo = null;

    const messages = ["WifiManager:setEnabled:Return:OK", "WifiManager:setEnabled:Return:NO",
                      "WifiManager:getNetworks:Return:OK", "WifiManager:getNetworks:Return:NO",
                      "WifiManager:associate:Return:OK", "WifiManager:associate:Return:NO",
                      "WifiManager:forget:Return:OK", "WifiManager:forget:Return:NO",
                      "WifiManager:onconnecting", "WifiManager:onassociate",
                      "WifiManager:onconnect", "WifiManager:ondisconnect",
                      "WifiManager:connectionInfoUpdate"];
    this.initHelper(aWindow, messages);
    this._mm = Cc["@mozilla.org/childprocessmessagemanager;1"].getService(Ci.nsISyncMessageSender);

    var state = this._mm.sendSyncMessage("WifiManager:getState")[0];
    if (state) {
      this._currentNetwork = state.network;
      this._lastConnectionInfo = state.connectionInfo;
      this._enabled = state.enabled;
      this._connectionStatus = state.status;
    } else {
      this._currentNetwork = null;
      this._lastConnectionInfo = null;
      this._enabled = false;
      this._connectionStatus = "disconnected";
    }
  },

  uninit: function() {
    this._onConnecting = null;
    this._onAssociate = null;
    this._onConnect = null;
    this._onDisconnect = null;
    this._onConnectionInfoUpdate = null;
  },

  _sendMessageForRequest: function(name, data, request) {
    let id = this.getRequestId(request);
    this._mm.sendAsyncMessage(name, { data: data, rid: id, mid: this._id });
  },

  receiveMessage: function(aMessage) {
    let msg = aMessage.json;
    if (msg.mid && msg.mid != this._id)
      return;

    let request;
    switch (aMessage.name) {
      case "WifiManager:setEnabled:Return:OK":
        request = this.takeRequest(msg.rid);
        this._enabled = msg.data;
        if (!this._enabled)
          this._currentNetwork = null;
        Services.DOMRequest.fireSuccess(request, true);
        break;

      case "WifiManager:setEnabled:Return:NO":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireError(request, "Unable to initialize wifi");
        break;

      case "WifiManager:getNetworks:Return:OK":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireSuccess(request, msg.data);
        break;

      case "WifiManager:getNetworks:Return:NO":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireError(request, "Unable to scan for networks");
        break;

      case "WifiManager:associate:Return:OK":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireSuccess(request, true);
        break;

      case "WifiManager:associate:Return:NO":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireError(request, "Unable to add the network");
        break;

      case "WifiManager:forget:Return:OK":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireSuccess(request, true);
        break;

      case "WifiManager:forget:Return:NO":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireError(request, msg.data);
        break;

      case "WifiManager:onconnecting":
        this._currentNetwork = msg.network;
        this._connectionStatus = "connecting";
        this._fireStatusChangeEvent();
        break;

      case "WifiManager:onassociate":
        this._currentNetwork = msg.network;
        this._connectionStatus = "associated";
        this._fireStatusChangeEvent();
        break;

      case "WifiManager:onconnect":
        this._currentNetwork = msg.network;
        this._connectionStatus = "connected";
        this._fireStatusChangeEvent();
        break;

      case "WifiManager:ondisconnect":
        this._currentNetwork = null;
        this._connectionStatus = "disconnected";
        this._lastConnectionInfo = null;
        this._fireStatusChangeEvent();
        break;

      case "WifiManager:connectionInfoUpdate":
        this._lastConnectionInfo = msg;
        this._fireConnectionInfoUpdate(msg);
        break;
    }
  },

  _fireStatusChangeEvent: function StatusChangeEvent() {
    if (this._onStatusChange) {
      var event = new WifiStatusChangeEvent(this._currentNetwork,
                                            this._connectionStatus);
      this._onStatusChange.handleEvent(event);
    }
  },

  _fireConnectionInfoUpdate: function connectionInfoUpdate(info) {
    if (this._onConnectionInfoUpdate) {
      var evt = new ConnectionInfoUpdate(this._currentNetwork,
                                         info.signalStrength,
                                         info.relSignalStrength,
                                         info.linkSpeed);
      this._onConnectionInfoUpdate.handleEvent(evt);
    }
  },

  // nsIDOMWifiManager
  setEnabled: function nsIDOMWifiManager_setEnabled(enabled) {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:setEnabled", enabled, request);
    return request;
  },

  getNetworks: function nsIDOMWifiManager_getNetworks() {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:getNetworks", null, request);
    return request;
  },

  associate: function nsIDOMWifiManager_associate(network) {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:associate", network, request);
    return request;
  },

  forget: function nsIDOMWifiManager_forget(network) {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:forget", network, request);
    return request;
  },

  get enabled() {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    return this._enabled;
  },

  get connection() {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    return { status: this._connectionStatus, network: this._currentNetwork };
  },

  get connectionInfo() {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    return this._lastConnectionInfo;
  },

  set onstatuschange(callback) {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    this._onStatusChange = callback;
  },

  set connectionInfoUpdate(callback) {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    this._onConnectionInfoUpdate = callback;
  }
};

function WifiStatusChangeEvent(network) {
  this.network = network;
}

WifiStatusChangeEvent.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMWifiStatusChangeEvent]),

  classInfo: XPCOMUtils.generateCI({classID: Components.ID("{f28c1ae7-4db7-4a4d-bb06-737eb04ad700}"),
                                    contractID: "@mozilla.org/wifi/statechange-event;1",
                                    interfaces: [Ci.nsIDOMWifiStatusChangeEvent],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT,
                                    classDescription: "Wifi State Change Event"})
};

function ConnectionInfoUpdate(network, signalStrength, relSignalStrength, linkSpeed) {
  this.network = network;
  this.signalStrength = signalStrength;
  this.relSignalStrength = relSignalStrength;
  this.linkSpeed = linkSpeed;
}

ConnectionInfoUpdate.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMWifiConnectionInfoEvent]),

  classInfo: XPCOMUtils.generateCI({classID: Components.ID("{aba4c481-7ea2-464a-b14c-7254a5c99454}"),
                                    contractID: "@mozilla.org/wifi/connectioninfo-event;1",
                                    interfaces: [Ci.nsIDOMWifiConnectionInfoEvent],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT,
                                    classDescription: "Wifi Connection Info Event"})
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([DOMWifiManager]);

let debug;
if (DEBUG) {
  debug = function (s) {
    dump("-*- DOMWifiManager component: " + s + "\n");
  };
} else {
  debug = function (s) {};
}
