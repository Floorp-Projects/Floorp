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

const DEBUG = true; // set to false to suppress debug messages

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
    this._enabled = true;

    const messages = ["WifiManager:setEnabled:Return:OK", "WifiManager:setEnabled:Return:NO",
                      "WifiManager:getNetworks:Return:OK", "WifiManager:getNetworks:Return:NO",
                      "WifiManager:associate:Return:OK", "WifiManager:associate:Return:NO",
                      "WifiManager:onconnecting", "WifiManager:onassociate",
                      "WifiManager:onconnect", "WifiManager:ondisconnect"];
    this.initHelper(aWindow, messages);
    this._mm = Cc["@mozilla.org/childprocessmessagemanager;1"].getService(Ci.nsISyncMessageSender);

    var state = this._mm.sendSyncMessage("WifiManager:getState");
    this._currentNetwork = state[0].network;
    this._enabled = state[0].enabled;
  },

  uninit: function() {
    this._onConnecting = null;
    this._onAssociate = null;
    this._onConnect = null;
    this._onDisconnect = null;
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

      case "WifiManager:onconnecting":
        this._currentNetwork = msg.network;
        this._fireOnConnecting(msg.network);
        break;

      case "WifiManager:onassociate":
        this._currentNetwork = msg.network;
        this._fireOnAssociate(msg.network);
        break;

      case "WifiManager:onconnect":
        this._currentNetwork = msg.network;
        this._fireOnConnect(msg.network);
        break;

      case "WifiManager:ondisconnect":
        this._fireOnDisconnect(this._currentNetwork);
        this._currentNetwork = null;
        break;
    }
  },

  _fireOnConnecting: function onConnecting(network) {
    if (this._onConnecting)
      this._onConnecting.handleEvent(new WifiStateChangeEvent(network));
  },

  _fireOnAssociate: function onAssociate(network) {
    if (this._onAssociate)
      this._onAssociate.handleEvent(new WifiStateChangeEvent(network));
  },

  _fireOnConnect: function onConnect(network) {
    if (this._onConnect)
      this._onConnect.handleEvent(new WifiStateChangeEvent(network));
  },

  _fireOnDisconnect: function onDisconnect(network) {
    if (this._onDisconnect) {
      this._onDisconnect.handleEvent(new WifiStateChangeEvent(network));
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

  get enabled() {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    return this._enabled;
  },

  get connectedNetwork() {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    return this._currentNetwork;
  },

  set onconnecting(callback) {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    this._onConnecting = callback;
  },

  set onassociate(callback) {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    this._onAssociate = callback;
  },

  set onconnect(callback) {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    this._onConnect = callback;
  },

  set ondisconnect(callback) {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    this._onDisconnect = callback;
  }
};

function WifiStateChangeEvent(network) {
  this.network = network;
}

WifiStateChangeEvent.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMWifiStateChangeEvent]),

  classInfo: XPCOMUtils.generateCI({classID: Components.ID("{f28c1ae7-4db7-4a4d-bb06-737eb04ad700}"),
                                    contractID: "@mozilla.org/wifi/statechange-event;1",
                                    interfaces: [Ci.nsIDOMWifiStateChangeEvent],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT,
                                    classDescription: "Wifi State Change Event"})
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
