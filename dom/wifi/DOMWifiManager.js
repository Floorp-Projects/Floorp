/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
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
const DOMWIFIMANAGER_CID        = Components.ID("{c9b5f09e-25d2-40ca-aef4-c4d13d93c706}");

function MozWifiNetwork() {
}

MozWifiNetwork.prototype = {

  init: function(aWindow) {
    this._window = aWindow;
  },

  __init: function(obj) {
    for (let key in obj) {
      this[key] = obj[key];
    }
  },

  classID: Components.ID("{c01fd751-43c0-460a-8b64-abf652ec7220}"),
  contractID: "@mozilla.org/mozwifinetwork;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer])
};

function MozWifiConnection(obj) {
  this.status = obj.status;
  this.network = obj.network;
}

MozWifiConnection.prototype = {
  classID: Components.ID("{23579da4-201b-4319-bd42-9b7f337343ac}"),
  contractID: "@mozilla.org/mozwificonnection;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports])
};

function MozWifiConnectionInfo(obj) {
  this.signalStrength = obj.signalStrength;
  this.relSignalStrength = obj.relSignalStrength;
  this.linkSpeed = obj.linkSpeed;
  this.ipAddress = obj.ipAddress;
}

MozWifiConnectionInfo.prototype = {
  classID: Components.ID("{83670352-6ed4-4c35-8de9-402296a1959c}"),
  contractID: "@mozilla.org/mozwificonnectioninfo;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports])
}

function MozWifiCapabilities(obj) {
  this.security = obj.security;
  this.eapMethod = obj.eapMethod;
  this.eapPhase2 = obj.eapPhase2;
  this.certificate = obj.certificate;
}

MozWifiCapabilities.prototype = {
  classID: Components.ID("08c88ece-8092-481b-863b-5515a52e411a"),
  contractID: "@mozilla.org/mozwificapabilities;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports])
}

function DOMWifiManager() {
  this.defineEventHandlerGetterSetter("onstatuschange");
  this.defineEventHandlerGetterSetter("onconnectioninfoupdate");
  this.defineEventHandlerGetterSetter("onenabled");
  this.defineEventHandlerGetterSetter("ondisabled");
  this.defineEventHandlerGetterSetter("onstationinfoupdate");
}

DOMWifiManager.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,
  classDescription: "DOMWifiManager",
  classID: DOMWIFIMANAGER_CID,
  contractID: DOMWIFIMANAGER_CONTRACTID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver]),

  // nsIDOMGlobalPropertyInitializer implementation
  init: function(aWindow) {
    // Maintain this state for synchronous APIs.
    this._currentNetwork = null;
    this._connectionStatus = "disconnected";
    this._enabled = false;
    this._lastConnectionInfo = null;
    this._capabilities = null;
    this._stationNumber = 0;

    const messages = ["WifiManager:getNetworks:Return:OK", "WifiManager:getNetworks:Return:NO",
                      "WifiManager:getKnownNetworks:Return:OK", "WifiManager:getKnownNetworks:Return:NO",
                      "WifiManager:associate:Return:OK", "WifiManager:associate:Return:NO",
                      "WifiManager:forget:Return:OK", "WifiManager:forget:Return:NO",
                      "WifiManager:wps:Return:OK", "WifiManager:wps:Return:NO",
                      "WifiManager:setPowerSavingMode:Return:OK", "WifiManager:setPowerSavingMode:Return:NO",
                      "WifiManager:setHttpProxy:Return:OK", "WifiManager:setHttpProxy:Return:NO",
                      "WifiManager:setStaticIpMode:Return:OK", "WifiManager:setStaticIpMode:Return:NO",
                      "WifiManager:importCert:Return:OK", "WifiManager:importCert:Return:NO",
                      "WifiManager:getImportedCerts:Return:OK", "WifiManager:getImportedCerts:Return:NO",
                      "WifiManager:deleteCert:Return:OK", "WifiManager:deleteCert:Return:NO",
                      "WifiManager:setWifiEnabled:Return:OK", "WifiManager:setWifiEnabled:Return:NO",
                      "WifiManager:wifiDown", "WifiManager:wifiUp",
                      "WifiManager:onconnecting", "WifiManager:onassociate",
                      "WifiManager:onconnect", "WifiManager:ondisconnect",
                      "WifiManager:onwpstimeout", "WifiManager:onwpsfail",
                      "WifiManager:onwpsoverlap", "WifiManager:connectioninfoupdate",
                      "WifiManager:onauthenticating", "WifiManager:onconnectingfailed",
                      "WifiManager:stationinfoupdate"];
    this.initDOMRequestHelper(aWindow, messages);
    this._mm = Cc["@mozilla.org/childprocessmessagemanager;1"].getService(Ci.nsISyncMessageSender);

    var state = this._mm.sendSyncMessage("WifiManager:getState")[0];
    if (state) {
      this._currentNetwork = this._convertWifiNetwork(state.network);
      this._lastConnectionInfo = this._convertConnectionInfo(state.connectionInfo);
      this._enabled = state.enabled;
      this._connectionStatus = state.status;
      this._macAddress = state.macAddress;
      this._capabilities = this._convertWifiCapabilities(state.capabilities);
    } else {
      this._currentNetwork = null;
      this._lastConnectionInfo = null;
      this._enabled = false;
      this._connectionStatus = "disconnected";
      this._macAddress = "";
    }
  },

  _convertWifiNetworkToJSON: function(aNetwork) {
    let json = {};

    for (let key in aNetwork) {
      // In WifiWorker.js there are lots of check using "key in network".
      // So if the value of any property of WifiNetwork is undefined, do not clone it.
      if (aNetwork[key] != undefined) {
        json[key] = aNetwork[key];
      }
    }
    return json;
  },

  _convertWifiNetwork: function(aNetwork) {
    let network = aNetwork ? new this._window.MozWifiNetwork(aNetwork) : null;
    return network;
  },

  _convertWifiNetworks: function(aNetworks) {
    let networks = [];
    for (let i in aNetworks) {
      networks.push(this._convertWifiNetwork(aNetworks[i]));
    }
    return networks;
  },

  _convertConnection: function(aConn) {
    let conn = aConn ? new MozWifiConnection(aConn) : null;
    return conn;
  },

  _convertConnectionInfo: function(aInfo) {
    let info = aInfo ? new MozWifiConnectionInfo(aInfo) : null;
    return info;
  },

  _convertWifiCapabilities: function(aCapabilities) {
    let capabilities = aCapabilities ?
                         new MozWifiCapabilities(aCapabilities) : null;
    return capabilities;
  },

  _genReadonlyPropDesc: function(value) {
    return {
      enumerable: true, configurable: false, writable: false, value: value
    };
  },

  _convertWifiCertificateInfo: function(aInfo) {
    let propList = {};
    for (let k in aInfo) {
      propList[k] = this._genReadonlyPropDesc(aInfo[k]);
    }

    let info = Cu.createObjectIn(this._window);
    Object.defineProperties(info, propList);
    Cu.makeObjectPropsNormal(info);

    return info;
  },

  _convertWifiCertificateList: function(aList) {
    let propList = {};
    for (let k in aList) {
      propList[k] = this._genReadonlyPropDesc(aList[k]);
    }

    let list = Cu.createObjectIn(this._window);
    Object.defineProperties(list, propList);
    Cu.makeObjectPropsNormal(list);

    return list;
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
    if (msg.rid) {
      request = this.takeRequest(msg.rid);
      if (!request) {
        return;
      }
    }

    switch (aMessage.name) {
      case "WifiManager:setWifiEnabled:Return:OK":
        Services.DOMRequest.fireSuccess(request, msg.data);
        break;

      case "WifiManager:setWifiEnabled:Return:NO":
        Services.DOMRequest.fireError(request, "Unable to enable/disable Wifi");
        break;

      case "WifiManager:getNetworks:Return:OK":
        Services.DOMRequest.fireSuccess(request, this._convertWifiNetworks(msg.data));
        break;

      case "WifiManager:getNetworks:Return:NO":
        Services.DOMRequest.fireError(request, "Unable to scan for networks");
        break;

      case "WifiManager:getKnownNetworks:Return:OK":
        Services.DOMRequest.fireSuccess(request, this._convertWifiNetworks(msg.data));
        break;

      case "WifiManager:getKnownNetworks:Return:NO":
        Services.DOMRequest.fireError(request, "Unable to get known networks");
        break;

      case "WifiManager:associate:Return:OK":
        Services.DOMRequest.fireSuccess(request, true);
        break;

      case "WifiManager:associate:Return:NO":
        Services.DOMRequest.fireError(request, "Unable to add the network");
        break;

      case "WifiManager:forget:Return:OK":
        Services.DOMRequest.fireSuccess(request, true);
        break;

      case "WifiManager:forget:Return:NO":
        Services.DOMRequest.fireError(request, msg.data);
        break;

      case "WifiManager:wps:Return:OK":
        Services.DOMRequest.fireSuccess(request, msg.data);
        break;

      case "WifiManager:wps:Return:NO":
        Services.DOMRequest.fireError(request, msg.data);
        break;

      case "WifiManager:setPowerSavingMode:Return:OK":
        Services.DOMRequest.fireSuccess(request, msg.data);
        break;

      case "WifiManager:setPowerSavingMode:Return:NO":
        Services.DOMRequest.fireError(request, msg.data);
        break;

      case "WifiManager:setHttpProxy:Return:OK":
        Services.DOMRequest.fireSuccess(request, msg.data);
        break;

      case "WifiManager:setHttpProxy:Return:NO":
        Services.DOMRequest.fireError(request, msg.data);
        break;

      case "WifiManager:setStaticIpMode:Return:OK":
        Services.DOMRequest.fireSuccess(request, msg.data);
        break;

      case "WifiManager:setStaticIpMode:Return:NO":
        Services.DOMRequest.fireError(request, msg.data);
        break;

      case "WifiManager:importCert:Return:OK":
        Services.DOMRequest.fireSuccess(request, this._convertWifiCertificateInfo(msg.data));
        break;

      case "WifiManager:importCert:Return:NO":
        Services.DOMRequest.fireError(request, msg.data);
        break;

      case "WifiManager:getImportedCerts:Return:OK":
        Services.DOMRequest.fireSuccess(request, this._convertWifiCertificateList(msg.data));
        break;

      case "WifiManager:getImportedCerts:Return:NO":
        Services.DOMRequest.fireError(request, msg.data);
        break;

      case "WifiManager:deleteCert:Return:OK":
        Services.DOMRequest.fireSuccess(request, msg.data);
        break;

      case "WifiManager:deleteCert:Return:NO":
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
        this._currentNetwork = this._convertWifiNetwork(msg.network);
        this._connectionStatus = "connecting";
        this._fireStatusChangeEvent();
        break;

      case "WifiManager:onassociate":
        this._currentNetwork = this._convertWifiNetwork(msg.network);
        this._connectionStatus = "associated";
        this._fireStatusChangeEvent();
        break;

      case "WifiManager:onconnect":
        this._currentNetwork = this._convertWifiNetwork(msg.network);
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

      case "WifiManager:connectioninfoupdate":
        this._lastConnectionInfo = this._convertConnectionInfo(msg);
        this._fireConnectionInfoUpdate(msg);
        break;
      case "WifiManager:onconnectingfailed":
        this._currentNetwork = null;
        this._connectionStatus = "connectingfailed";
        this._lastConnectionInfo = null;
        this._fireStatusChangeEvent();
        break;
      case "WifiManager:onauthenticating":
        this._currentNetwork = msg.network;
        this._connectionStatus = "authenticating";
        this._fireStatusChangeEvent();
        break;
      case "WifiManager:stationinfoupdate":
        this._stationNumber = msg.station;
        this._fireStationInfoUpdate(msg);
        break;
    }
  },

  _fireStatusChangeEvent: function StatusChangeEvent() {
    var event = new this._window.MozWifiStatusChangeEvent("statuschange",
                                                          { network: this._currentNetwork,
                                                            status: this._connectionStatus
                                                          });
    this.__DOM_IMPL__.dispatchEvent(event);
  },

  _fireConnectionInfoUpdate: function onConnectionInfoUpdate(info) {
    var evt = new this._window.MozWifiConnectionInfoEvent("connectioninfoupdate",
                                                          { network: this._currentNetwork,
                                                            signalStrength: info.signalStrength,
                                                            relSignalStrength: info.relSignalStrength,
                                                            linkSpeed: info.linkSpeed,
                                                            ipAddress: info.ipAddress,
                                                          });
    this.__DOM_IMPL__.dispatchEvent(evt);
  },

  _fireEnabledOrDisabled: function enabledDisabled(enabled) {
    var evt = new this._window.Event(enabled ? "enabled" : "disabled");
    this.__DOM_IMPL__.dispatchEvent(evt);
  },

  _fireStationInfoUpdate: function onStationInfoUpdate(info) {
    var evt = new this._window.MozWifiStationInfoEvent("stationinfoupdate",
                                                       { station: this._stationNumber}
                                                      );
    this.__DOM_IMPL__.dispatchEvent(evt);
  },

  setWifiEnabled: function setWifiEnabled(enabled) {
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:setWifiEnabled", enabled, request);
    return request;
  },

  getNetworks: function getNetworks() {
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:getNetworks", null, request);
    return request;
  },

  getKnownNetworks: function getKnownNetworks() {
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:getKnownNetworks", null, request);
    return request;
  },

  associate: function associate(network) {
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:associate",
                                this._convertWifiNetworkToJSON(network), request);
    return request;
  },

  forget: function forget(network) {
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:forget",
                                this._convertWifiNetworkToJSON(network), request);
    return request;
  },

  wps: function wps(detail) {
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:wps", detail, request);
    return request;
  },

  setPowerSavingMode: function setPowerSavingMode(enabled) {
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:setPowerSavingMode", enabled, request);
    return request;
  },

  setHttpProxy: function setHttpProxy(network, info) {
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:setHttpProxy",
                                { network: this._convertWifiNetworkToJSON(network), info:info}, request);
    return request;
  },

  setStaticIpMode: function setStaticIpMode(network, info) {
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:setStaticIpMode",
                                { network: this._convertWifiNetworkToJSON(network), info: info}, request);
    return request;
  },

  importCert: function nsIDOMWifiManager_importCert(certBlob, certPassword, certNickname) {
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:importCert",
                                {
                                  certBlob: certBlob,
                                  certPassword: certPassword,
                                  certNickname: certNickname
                                }, request);
    return request;
  },

  getImportedCerts: function nsIDOMWifiManager_getImportedCerts() {
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:getImportedCerts", null, request);
    return request;
  },

  deleteCert: function nsIDOMWifiManager_deleteCert(certNickname) {
    var request = this.createRequest();
    this._sendMessageForRequest("WifiManager:deleteCert",
                                {
                                  certNickname: certNickname
                                }, request);
    return request;
  },

  get enabled() {
    return this._enabled;
  },

  get macAddress() {
    return this._macAddress;
  },

  get connection() {
    let _connection = this._convertConnection({ status: this._connectionStatus,
                                                network: this._currentNetwork,
                                              });
    return _connection;
  },

  get connectionInformation() {
    return this._lastConnectionInfo;
  },

  get capabilities() {
    return this._capabilities;
  },

  defineEventHandlerGetterSetter: function(name) {
    Object.defineProperty(this, name, {
      get: function() {
        return this.__DOM_IMPL__.getEventHandler(name);
      },
      set: function(handler) {
        this.__DOM_IMPL__.setEventHandler(name, handler);
      }
    });
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([
  DOMWifiManager, MozWifiNetwork, MozWifiConnection, MozWifiCapabilities,
  MozWifiConnectionInfo
]);

let debug;
if (DEBUG) {
  debug = function (s) {
    dump("-*- DOMWifiManager component: " + s + "\n");
  };
} else {
  debug = function (s) {};
}
