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

function exposeCurrentNetwork(currentNetwork) {
  currentNetwork.__exposedProps__ = exposeCurrentNetwork.currentNetworkApi;
}

exposeCurrentNetwork.currentNetworkApi = {
  ssid: "r",
  security: "r",
  capabilities: "r",
  known: "r"
};

// For smaller, read-only APIs, we expose any property that doesn't begin with
// an underscore.
function exposeReadOnly(obj) {
  var exposedProps = {};
  for (let i in obj) {
    if (i[0] === "_")
      continue;
    exposedProps[i] = "r";
  }

  obj.__exposedProps__ = exposedProps;
  return obj;
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
                                         Ci.nsIDOMGlobalPropertyInitializer,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver]),

  // nsIDOMGlobalPropertyInitializer implementation
  init: function(aWindow) {
    let principal = aWindow.document.nodePrincipal;
    let secMan = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(Ci.nsIScriptSecurityManager);

    let perm = principal == secMan.getSystemPrincipal()
                 ? Ci.nsIPermissionManager.ALLOW_ACTION
                 : Services.perms.testExactPermissionFromPrincipal(principal, "wifi-manage");

    // Only pages with perm set can use the wifi manager.
    this._hasPrivileges = perm == Ci.nsIPermissionManager.ALLOW_ACTION;

    if (!this._hasPrivileges) {
      return null;
    }

    // Maintain this state for synchronous APIs.
    this._currentNetwork = null;
    this._connectionStatus = "disconnected";
    this._enabled = false;
    this._lastConnectionInfo = null;

    const messages = ["WifiManager:getNetworks:Return:OK", "WifiManager:getNetworks:Return:NO",
                      "WifiManager:getKnownNetworks:Return:OK", "WifiManager:getKnownNetworks:Return:NO",
                      "WifiManager:associate:Return:OK", "WifiManager:associate:Return:NO",
                      "WifiManager:forget:Return:OK", "WifiManager:forget:Return:NO",
                      "WifiManager:wps:Return:OK", "WifiManager:wps:Return:NO",
                      "WifiManager:setPowerSavingMode:Return:OK", "WifiManager:setPowerSavingMode:Return:NO",
                      "WifiManager:setHttpProxy:Return:OK", "WifiManager:setHttpProxy:Return:NO",
                      "WifiManager:setStaticIpMode:Return:OK", "WifiManager:setStaticIpMode:Return:NO",
                      "WifiManager:wifiDown", "WifiManager:wifiUp",
                      "WifiManager:onconnecting", "WifiManager:onassociate",
                      "WifiManager:onconnect", "WifiManager:ondisconnect",
                      "WifiManager:onwpstimeout", "WifiManager:onwpsfail",
                      "WifiManager:onwpsoverlap", "WifiManager:connectionInfoUpdate",
                      "WifiManager:onconnectingfailed"];
    this.initDOMRequestHelper(aWindow, messages);
    this._mm = Cc["@mozilla.org/childprocessmessagemanager;1"].getService(Ci.nsISyncMessageSender);

    var state = this._mm.sendSyncMessage("WifiManager:getState")[0];
    if (state) {
      this._currentNetwork = state.network;
      if (this._currentNetwork)
        exposeCurrentNetwork(this._currentNetwork);
      this._lastConnectionInfo = state.connectionInfo;
      this._enabled = state.enabled;
      this._connectionStatus = state.status;
      this._macAddress = state.macAddress;
    } else {
      this._currentNetwork = null;
      this._lastConnectionInfo = null;
      this._enabled = false;
      this._connectionStatus = "disconnected";
      this._macAddress = "";
    }
  },

  uninit: function() {
    this._onStatusChange = null;
    this._onConnectionInfoUpdate = null;
    this._onEnabled = null;
    this._onDisabled = null;
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
      case "WifiManager:getNetworks:Return:OK":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireSuccess(request, exposeReadOnly(msg.data));
        break;

      case "WifiManager:getNetworks:Return:NO":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireError(request, "Unable to scan for networks");
        break;

      case "WifiManager:getKnownNetworks:Return:OK":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireSuccess(request, exposeReadOnly(msg.data));
        break;

      case "WifiManager:getKnownNetworks:Return:NO":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireError(request, "Unable to get known networks");
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

      case "WifiManager:wps:Return:OK":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireSuccess(request, exposeReadOnly(msg.data));
        break;

      case "WifiManager:wps:Return:NO":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireError(request, msg.data);
        break;

      case "WifiManager:setPowerSavingMode:Return:OK":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireSuccess(request, exposeReadOnly(msg.data));
        break;

      case "WifiManager:setPowerSavingMode:Return:NO":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireError(request, msg.data);
        break;

      case "WifiManager:setHttpProxy:Return:OK":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireSuccess(request, exposeReadOnly(msg.data));
        break;

      case "WifiManager:setHttpProxy:Return:NO":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireError(request, msg.data);
        break;

      case "WifiManager:setStaticIpMode:Return:OK":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireSuccess(request, exposeReadOnly(msg.data));
        break;

      case "WifiManager:setStaticIpMode:Return:NO":
        request = this.takeRequest(msg.rid);
        Services.DOMRequest.fireError(request, msg.data);
        break;

      case "WifiManager:wifiDown":
        this._enabled = false;
        this._currentNetwork = null;
        this._fireEnabledOrDisabled(false);
        break;

      case "WifiManager:wifiUp":
        this._enabled = true;
        this._macAddress = msg.macAddress;
        this._fireEnabledOrDisabled(true);
        break;

      case "WifiManager:onconnecting":
        this._currentNetwork = msg.network;
        exposeCurrentNetwork(this._currentNetwork);
        this._connectionStatus = "connecting";
        this._fireStatusChangeEvent();
        break;

      case "WifiManager:onassociate":
        this._currentNetwork = msg.network;
        exposeCurrentNetwork(this._currentNetwork);
        this._connectionStatus = "associated";
        this._fireStatusChangeEvent();
        break;

      case "WifiManager:onconnect":
        this._currentNetwork = msg.network;
        exposeCurrentNetwork(this._currentNetwork);
        this._connectionStatus = "connected";
        this._fireStatusChangeEvent();
        break;

      case "WifiManager:ondisconnect":
        this._currentNetwork = null;
        this._connectionStatus = "disconnected";
        this._lastConnectionInfo = null;
        this._fireStatusChangeEvent();
        break;

      case "WifiManager:onwpstimeout":
        this._currentNetwork = null;
        this._connectionStatus = "wps-timedout";
        this._lastConnectionInfo = null;
        this._fireStatusChangeEvent();
        break;

      case "WifiManager:onwpsfail":
        this._currentNetwork = null;
        this._connectionStatus = "wps-failed";
        this._lastConnectionInfo = null;
        this._fireStatusChangeEvent();
        break;

      case "WifiManager:onwpsoverlap":
        this._currentNetwork = null;
        this._connectionStatus = "wps-overlapped";
        this._lastConnectionInfo = null;
        this._fireStatusChangeEvent();
        break;

      case "WifiManager:connectionInfoUpdate":
        this._lastConnectionInfo = msg;
        this._fireConnectionInfoUpdate(msg);
        break;
      case "WifiManager:onconnectingfailed":
        this._currentNetwork = null;
        this._connectionStatus = "connectingfailed";
        this._lastConnectionInfo = null;
        this._fireStatusChangeEvent();
        break;
    }
  },

  _fireStatusChangeEvent: function StatusChangeEvent() {
    if (this._onStatusChange) {
      debug("StatusChangeEvent");
      var event = new this._window.MozWifiStatusChangeEvent("statusChangeEvent",
                                                            { network: this._currentNetwork,
                                                              status: this._connectionStatus
                                                            });
      this._onStatusChange.handleEvent(event);
    }
  },

  _fireConnectionInfoUpdate: function connectionInfoUpdate(info) {
    if (this._onConnectionInfoUpdate) {
      debug("ConnectionInfoEvent");
      var evt = new this._window.MozWifiConnectionInfoEvent("connectionInfoEvent",
                                                            { network: this._currentNetwork,
                                                              signalStrength: info.signalStrength,
                                                              relSignalStrength: info.relSignalStrength,
                                                              linkSpeed: info.linkSpeed,
                                                              ipAddress: info.ipAddress,
                                                            });
      this._onConnectionInfoUpdate.handleEvent(evt);
    }
  },

  _fireEnabledOrDisabled: function enabledDisabled(enabled) {
    var handler = enabled ? this._onEnabled : this._onDisabled;
    if (handler) {
      var evt = new this._window.Event("WifiEnabled");
      handler.handleEvent(evt);
    }
  },

  // nsIDOMWifiManager
  getNetworks: function nsIDOMWifiManager_getNetworks() {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:getNetworks", null, request);
    return request;
  },

  getKnownNetworks: function nsIDOMWifiManager_getKnownNetworks() {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:getKnownNetworks", null, request);
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

  wps: function nsIDOMWifiManager_wps(detail) {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:wps", detail, request);
    return request;
  },

  setPowerSavingMode: function nsIDOMWifiManager_setPowerSavingMode(enabled) {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:setPowerSavingMode", enabled, request);
    return request;
  },

  setHttpProxy: function nsIDOMWifiManager_setHttpProxy(network, info) {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:setHttpProxy", {network:network, info:info}, request);
    return request;
  },

  setStaticIpMode: function nsIDOMWifiManager_setStaticIpMode(network, info) {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:setStaticIpMode", {network: network,info: info}, request);
    return request;
  },

  get enabled() {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    return this._enabled;
  },

  get macAddress() {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    return this._macAddress;
  },

  get connection() {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    return exposeReadOnly({ status: this._connectionStatus,
                            network: this._currentNetwork });
  },

  get connectionInformation() {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    return this._lastConnectionInfo
           ? exposeReadOnly(this._lastConnectionInfo)
           : null;
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
  },

  set onenabled(callback) {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    this._onEnabled = callback;
  },

  set ondisabled(callback) {
    if (!this._hasPrivileges)
      throw new Components.Exception("Denied", Cr.NS_ERROR_FAILURE);
    this._onDisabled = callback;
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DOMWifiManager]);

let debug;
if (DEBUG) {
  debug = function (s) {
    dump("-*- DOMWifiManager component: " + s + "\n");
  };
} else {
  debug = function (s) {};
}
