/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* jshint esnext:true, globalstrict:true, moz:true, undef:true, unused:true */
/* globals Components, dump */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

/* globals XPCOMUtils */
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
/* globals Services */
Cu.import("resource://gre/modules/Services.jsm");
/* globals NetUtil */
Cu.import("resource://gre/modules/NetUtil.jsm");

const DEBUG = Services.prefs.getBoolPref("dom.presentation.tcp_server.debug");
function log(aMsg) {
  dump("-*- LegacyPresentationControlService.js: " + aMsg + "\n");
}

function LegacyPresentationControlService() {
  DEBUG && log("LegacyPresentationControlService - ctor"); //jshint ignore:line
  this._id = null;
}

LegacyPresentationControlService.prototype = {
  startServer: function() {
    DEBUG && log("LegacyPresentationControlService - doesn't support receiver mode"); //jshint ignore:line
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  get id() {
    return this._id;
  },

  set id(aId) {
    this._id = aId;
  },

  get port() {
    return 0;
  },

  get version() {
    return 0;
  },

  set listener(aListener) { //jshint ignore:line
    DEBUG && log("LegacyPresentationControlService - doesn't support receiver mode"); //jshint ignore:line
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  get listener() {
    return null;
  },

  connect: function(aDeviceInfo) {
    if (!this.id) {
      DEBUG && log("LegacyPresentationControlService - Id has not initialized; requestSession fails"); //jshint ignore:line
      return null;
    }
    DEBUG && log("LegacyPresentationControlService - requestSession to " + aDeviceInfo.id); //jshint ignore:line

    let sts = Cc["@mozilla.org/network/socket-transport-service;1"]
                .getService(Ci.nsISocketTransportService);

    let socketTransport;
    try {
      socketTransport = sts.createTransport(null,
                                            0,
                                            aDeviceInfo.address,
                                            aDeviceInfo.port,
                                            null);
    } catch (e) {
      DEBUG && log("LegacyPresentationControlService - createTransport throws: " + e); //jshint ignore:line
      // Pop the exception to |TCPDevice.establishControlChannel|
      throw Cr.NS_ERROR_FAILURE;
    }
    return new LegacyTCPControlChannel(this.id,
                                       socketTransport,
                                       aDeviceInfo);
  },

  close: function() {
    DEBUG && log("LegacyPresentationControlService - close"); //jshint ignore:line
  },

  classID: Components.ID("{b21816fe-8aff-4811-86d2-85a7444c557e}"),
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIPresentationControlService]),
};

function ChannelDescription(aInit) {
  this._type = aInit.type;
  switch (this._type) {
    case Ci.nsIPresentationChannelDescription.TYPE_TCP:
      this._tcpAddresses = Cc["@mozilla.org/array;1"]
                           .createInstance(Ci.nsIMutableArray);
      for (let address of aInit.tcpAddress) {
        let wrapper = Cc["@mozilla.org/supports-cstring;1"]
                      .createInstance(Ci.nsISupportsCString);
        wrapper.data = address;
        this._tcpAddresses.appendElement(wrapper, false);
      }

      this._tcpPort = aInit.tcpPort;
      break;
    case Ci.nsIPresentationChannelDescription.TYPE_DATACHANNEL:
      this._dataChannelSDP = aInit.dataChannelSDP;
      break;
  }
}

ChannelDescription.prototype = {
  _type: 0,
  _tcpAddresses: null,
  _tcpPort: 0,
  _dataChannelSDP: "",

  get type() {
    return this._type;
  },

  get tcpAddress() {
    return this._tcpAddresses;
  },

  get tcpPort() {
    return this._tcpPort;
  },

  get dataChannelSDP() {
    return this._dataChannelSDP;
  },

  classID: Components.ID("{d69fc81c-4f40-47a3-97e6-b4cf5db2294e}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationChannelDescription]),
};

// Helper function: transfer nsIPresentationChannelDescription to json
function discriptionAsJson(aDescription) {
  let json = {};
  json.type = aDescription.type;
  switch(aDescription.type) {
    case Ci.nsIPresentationChannelDescription.TYPE_TCP:
      let addresses = aDescription.tcpAddress.QueryInterface(Ci.nsIArray);
      json.tcpAddress = [];
      for (let idx = 0; idx < addresses.length; idx++) {
        let address = addresses.queryElementAt(idx, Ci.nsISupportsCString);
        json.tcpAddress.push(address.data);
      }
      json.tcpPort = aDescription.tcpPort;
      break;
    case Ci.nsIPresentationChannelDescription.TYPE_DATACHANNEL:
      json.dataChannelSDP = aDescription.dataChannelSDP;
      break;
  }
  return json;
}

function LegacyTCPControlChannel(id,
                                 transport,
                                 deviceInfo) {
  DEBUG && log("create LegacyTCPControlChannel"); //jshint ignore:line
  this._deviceInfo = deviceInfo;
  this._transport = transport;

  this._id = id;

  let currentThread = Services.tm.currentThread;
  transport.setEventSink(this, currentThread);

  this._input = this._transport.openInputStream(0, 0, 0)
                               .QueryInterface(Ci.nsIAsyncInputStream);
  this._input.asyncWait(this.QueryInterface(Ci.nsIStreamListener),
                        Ci.nsIAsyncInputStream.WAIT_CLOSURE_ONLY,
                        0,
                        currentThread);

  this._output = this._transport
                     .openOutputStream(Ci.nsITransport.OPEN_UNBUFFERED, 0, 0);
}

LegacyTCPControlChannel.prototype = {
  _connected: false,
  _pendingOpen: false,
  _pendingAnswer: null,
  _pendingClose: null,
  _pendingCloseReason: null,

  _sendMessage: function(aJSONData, aOnThrow) {
    if (!aOnThrow) {
      aOnThrow = function(e) {throw e.result;};
    }

    if (!aJSONData) {
      aOnThrow();
      return;
    }

    if (!this._connected) {
      DEBUG && log("LegacyTCPControlChannel - send" + aJSONData.type + " fails"); //jshint ignore:line
      throw Cr.NS_ERROR_FAILURE;
    }

    try {
      this._send(aJSONData);
    } catch (e) {
      aOnThrow(e);
    }
  },

  _sendInit: function() {
    let msg = {
      type: "requestSession:Init",
      presentationId: this._presentationId,
      url: this._url,
      id: this._id,
    };

    this._sendMessage(msg, function(e) {
      this.disconnect();
      this._notifyDisconnected(e.result);
    });
  },

  launch: function(aPresentationId, aUrl) {
    this._presentationId = aPresentationId;
    this._url = aUrl;

    this._sendInit();
  },

  terminate: function() {
    // Legacy protocol doesn't support extra terminate protocol.
    // Trigger error handling for browser to shutdown all the resource locally.
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  sendOffer: function(aOffer) {
    let msg = {
      type: "requestSession:Offer",
      presentationId: this._presentationId,
      offer: discriptionAsJson(aOffer),
    };
    this._sendMessage(msg);
  },

  sendAnswer: function(aAnswer) { //jshint ignore:line
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  sendIceCandidate: function(aCandidate) {
    let msg = {
      type: "requestSession:IceCandidate",
      presentationId: this._presentationId,
      iceCandidate: aCandidate,
    };
    this._sendMessage(msg);
  },
  // may throw an exception
  _send: function(aMsg) {
    DEBUG && log("LegacyTCPControlChannel - Send: " + JSON.stringify(aMsg, null, 2)); //jshint ignore:line

    /**
     * XXX In TCP streaming, it is possible that more than one message in one
     * TCP packet. We use line delimited JSON to identify where one JSON encoded
     * object ends and the next begins. Therefore, we do not allow newline
     * characters whithin the whole message, and add a newline at the end.
     * Please see the parser code in |onDataAvailable|.
     */
    let message = JSON.stringify(aMsg).replace(["\n"], "") + "\n";
    try {
      this._output.write(message, message.length);
    } catch(e) {
      DEBUG && log("LegacyTCPControlChannel - Failed to send message: " + e.name); //jshint ignore:line
      throw e;
    }
  },

  // nsIAsyncInputStream (Triggered by nsIInputStream.asyncWait)
  // Only used for detecting connection refused
  onInputStreamReady: function(aStream) {
    try {
      aStream.available();
    } catch (e) {
      DEBUG && log("LegacyTCPControlChannel - onInputStreamReady error: " + e.name); //jshint ignore:line
      // NS_ERROR_CONNECTION_REFUSED
      this._listener.notifyDisconnected(e.result);
    }
  },

  // nsITransportEventSink (Triggered by nsISocketTransport.setEventSink)
  onTransportStatus: function(aTransport, aStatus, aProg, aProgMax) { //jshint ignore:line
    DEBUG && log("LegacyTCPControlChannel - onTransportStatus: "
                 + aStatus.toString(16)); //jshint ignore:line
    if (aStatus === Ci.nsISocketTransport.STATUS_CONNECTED_TO) {
      this._connected = true;

      if (!this._pump) {
        this._createInputStreamPump();
      }

      this._notifyConnected();
    }
  },

  // nsIRequestObserver (Triggered by nsIInputStreamPump.asyncRead)
  onStartRequest: function() {
    DEBUG && log("LegacyTCPControlChannel - onStartRequest"); //jshint ignore:line
  },

  // nsIRequestObserver (Triggered by nsIInputStreamPump.asyncRead)
  onStopRequest: function(aRequest, aContext, aStatus) {
    DEBUG && log("LegacyTCPControlChannel - onStopRequest: " + aStatus); //jshint ignore:line
    this.disconnect(aStatus);
    this._notifyDisconnected(aStatus);
  },

  // nsIStreamListener (Triggered by nsIInputStreamPump.asyncRead)
  onDataAvailable: function(aRequest, aContext, aInputStream, aOffset, aCount) { //jshint ignore:line
    let data = NetUtil.readInputStreamToString(aInputStream,
                                               aInputStream.available());
    DEBUG && log("LegacyTCPControlChannel - onDataAvailable: " + data); //jshint ignore:line

    // Parser of line delimited JSON. Please see |_send| for more informaiton.
    let jsonArray = data.split("\n");
    jsonArray.pop();
    for (let json of jsonArray) {
      let msg;
      try {
        msg = JSON.parse(json);
      } catch (e) {
        DEBUG && log("LegacyTCPSignalingChannel - error in parsing json: " + e); //jshint ignore:line
      }

      this._handleMessage(msg);
    }
  },

  _createInputStreamPump: function() {
    DEBUG && log("LegacyTCPControlChannel - create pump"); //jshint ignore:line
    this._pump = Cc["@mozilla.org/network/input-stream-pump;1"].
               createInstance(Ci.nsIInputStreamPump);
    this._pump.init(this._input, -1, -1, 0, 0, false);
    this._pump.asyncRead(this, null);
  },

  // Handle command from remote side
  _handleMessage: function(aMsg) {
    DEBUG && log("LegacyTCPControlChannel - handleMessage from "
                 + JSON.stringify(this._deviceInfo) + ": " + JSON.stringify(aMsg)); //jshint ignore:line
    switch (aMsg.type) {
      case "requestSession:Answer": {
        this._onAnswer(aMsg.answer);
        break;
      }
      case "requestSession:IceCandidate": {
        this._listener.onIceCandidate(aMsg.iceCandidate);
        break;
      }
      case "requestSession:CloseReason": {
        this._pendingCloseReason = aMsg.reason;
        break;
      }
    }
  },

  get listener() {
    return this._listener;
  },

  set listener(aListener) {
    DEBUG && log("LegacyTCPControlChannel - set listener: " + aListener); //jshint ignore:line
    if (!aListener) {
      this._listener = null;
      return;
    }

    this._listener = aListener;
    if (this._pendingOpen) {
      this._pendingOpen = false;
      DEBUG && log("LegacyTCPControlChannel - notify pending opened"); //jshint ignore:line
      this._listener.notifyConnected();
    }

    if (this._pendingAnswer) {
      let answer = this._pendingAnswer;
      DEBUG && log("LegacyTCPControlChannel - notify pending answer: " +
                   JSON.stringify(answer)); // jshint ignore:line
      this._listener.onAnswer(new ChannelDescription(answer));
      this._pendingAnswer = null;
    }

    if (this._pendingClose) {
      DEBUG && log("LegacyTCPControlChannel - notify pending closed"); //jshint ignore:line
      this._notifyDisconnected(this._pendingCloseReason);
      this._pendingClose = null;
    }
  },

  /**
   * These functions are designed to handle the interaction with listener
   * appropriately. |_FUNC| is to handle |this._listener.FUNC|.
   */
  _onAnswer: function(aAnswer) {
    if (!this._connected) {
      return;
    }
    if (!this._listener) {
      this._pendingAnswer = aAnswer;
      return;
    }
    DEBUG && log("LegacyTCPControlChannel - notify answer: " + JSON.stringify(aAnswer)); //jshint ignore:line
    this._listener.onAnswer(new ChannelDescription(aAnswer));
  },

  _notifyConnected: function() {
    this._connected = true;
    this._pendingClose = false;
    this._pendingCloseReason = Cr.NS_OK;

    if (!this._listener) {
      this._pendingOpen = true;
      return;
    }

    DEBUG && log("LegacyTCPControlChannel - notify opened"); //jshint ignore:line
    this._listener.notifyConnected();
  },

  _notifyDisconnected: function(aReason) {
    this._connected = false;
    this._pendingOpen = false;
    this._pendingAnswer = null;

    // Remote endpoint closes the control channel with abnormal reason.
    if (aReason == Cr.NS_OK && this._pendingCloseReason != Cr.NS_OK) {
      aReason = this._pendingCloseReason;
    }

    if (!this._listener) {
      this._pendingClose = true;
      this._pendingCloseReason = aReason;
      return;
    }

    DEBUG && log("LegacyTCPControlChannel - notify closed"); //jshint ignore:line
    this._listener.notifyDisconnected(aReason);
  },

  disconnect: function(aReason) {
    DEBUG && log("LegacyTCPControlChannel - close with reason: " + aReason); //jshint ignore:line

    if (this._connected) {
      // default reason is NS_OK
      if (typeof aReason !== "undefined" && aReason !== Cr.NS_OK) {
        let msg = {
          type: "requestSession:CloseReason",
          presentationId: this._presentationId,
          reason: aReason,
        };
        this._sendMessage(msg);
        this._pendingCloseReason = aReason;
      }

      this._transport.setEventSink(null, null);
      this._pump = null;

      this._input.close();
      this._output.close();

      this._connected = false;
    }
  },

  reconnect: function() {
    // Legacy protocol doesn't support extra reconnect protocol.
    // Trigger error handling for browser to shutdown all the resource locally.
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  classID: Components.ID("{4027ce3d-06e3-4d06-a235-df329cb0d411}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationControlChannel,
                                         Ci.nsIStreamListener]),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([LegacyPresentationControlService]); //jshint ignore:line
