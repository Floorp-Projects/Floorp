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

/* globals ControllerStateMachine */
XPCOMUtils.defineLazyModuleGetter(this, "ControllerStateMachine", // jshint ignore:line
                                  "resource://gre/modules/presentation/ControllerStateMachine.jsm");
/* global ReceiverStateMachine */
XPCOMUtils.defineLazyModuleGetter(this, "ReceiverStateMachine", // jshint ignore:line
                                  "resource://gre/modules/presentation/ReceiverStateMachine.jsm");

const kProtocolVersion = 1; // need to review isCompatibleServer while fiddling the version number.

const DEBUG = Services.prefs.getBoolPref("dom.presentation.tcp_server.debug");
function log(aMsg) {
  dump("-*- PresentationControlService.js: " + aMsg + "\n");
}

function TCPDeviceInfo(aAddress, aPort, aId) {
  this.address = aAddress;
  this.port = aPort;
  this.id = aId;
}

function PresentationControlService() {
  this._id = null;
  this._port = 0;
  this._serverSocket = null;
}

PresentationControlService.prototype = {
  /**
   * If a user agent connects to this server, we create a control channel but
   * hand it to |TCPDevice.listener| when the initial information exchange
   * finishes. Therefore, we hold the control channels in this period.
   */
  _controlChannels: [],

  startServer: function(aPort) {
    if (this._isServiceInit()) {
      DEBUG && log("PresentationControlService - server socket has been initialized");  // jshint ignore:line
      throw Cr.NS_ERROR_FAILURE;
    }

    /**
     * 0 or undefined indicates opt-out parameter, and a port will be selected
     * automatically.
     */
    let serverSocketPort = (typeof aPort !== "undefined" && aPort !== 0) ? aPort : -1;

    this._serverSocket = Cc["@mozilla.org/network/server-socket;1"]
                         .createInstance(Ci.nsIServerSocket);

    if (!this._serverSocket) {
      DEBUG && log("PresentationControlService - create server socket fail."); // jshint ignore:line
      throw Cr.NS_ERROR_FAILURE;
    }

    try {
      this._serverSocket.init(serverSocketPort, false, -1);
      this._serverSocket.asyncListen(this);
    } catch (e) {
      // NS_ERROR_SOCKET_ADDRESS_IN_USE
      DEBUG && log("PresentationControlService - init server socket fail: " + e); // jshint ignore:line
      throw Cr.NS_ERROR_FAILURE;
    }

    this._port = this._serverSocket.port;

    DEBUG && log("PresentationControlService - service start on port: " + this._port); // jshint ignore:line

    // Monitor network interface change to restart server socket.
    // Only B2G has nsINetworkManager
    Services.obs.addObserver(this, "network-active-changed", false);
    Services.obs.addObserver(this, "network:offline-status-changed", false);
  },

  isCompatibleServer: function(aVersion) {
    // No compatibility issue for the first version of control protocol
    return this.version === aVersion;
  },

  get id() {
    return this._id;
  },

  set id(aId) {
    this._id = aId;
  },

  get port() {
    return this._port;
  },

  get version() {
    return kProtocolVersion;
  },

  set listener(aListener) {
    this._listener = aListener;
  },

  get listener() {
    return this._listener;
  },

  _isServiceInit: function() {
    return this._serverSocket !== null;
  },

  connect: function(aDeviceInfo) {
    if (!this.id) {
      DEBUG && log("PresentationControlService - Id has not initialized; connect fails"); // jshint ignore:line
      return null;
    }
    DEBUG && log("PresentationControlService - connect to " + aDeviceInfo.id); // jshint ignore:line

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
      DEBUG && log("PresentationControlService - createTransport throws: " + e);  // jshint ignore:line
      // Pop the exception to |TCPDevice.establishControlChannel|
      throw Cr.NS_ERROR_FAILURE;
    }
    return new TCPControlChannel(this,
                                 socketTransport,
                                 aDeviceInfo,
                                 "sender");
  },

  responseSession: function(aDeviceInfo, aSocketTransport) {
    if (!this._isServiceInit()) {
      DEBUG && log("PresentationControlService - should never receive remote " +
                   "session request before server socket initialization"); // jshint ignore:line
      return null;
    }
    DEBUG && log("PresentationControlService - responseSession to " +
                 JSON.stringify(aDeviceInfo)); // jshint ignore:line
    return new TCPControlChannel(this,
                                 aSocketTransport,
                                 aDeviceInfo,
                                 "receiver");
  },

  // Triggered by TCPControlChannel
  onSessionRequest: function(aDeviceInfo, aUrl, aPresentationId, aControlChannel) {
    DEBUG && log("PresentationControlService - onSessionRequest: " +
                 aDeviceInfo.address + ":" + aDeviceInfo.port); // jshint ignore:line
    if (!this.listener) {
      this.releaseControlChannel(aControlChannel);
      return;
    }

    this.listener.onSessionRequest(aDeviceInfo,
                                   aUrl,
                                   aPresentationId,
                                   aControlChannel);
    this.releaseControlChannel(aControlChannel);
  },

  onSessionTerminate: function(aDeviceInfo, aPresentationId, aControlChannel, aIsFromReceiver) {
    DEBUG && log("TCPPresentationServer - onSessionTerminate: " +
                 aDeviceInfo.address + ":" + aDeviceInfo.port); // jshint ignore:line
    if (!this.listener) {
      this.releaseControlChannel(aControlChannel);
      return;
    }

    this.listener.onTerminateRequest(aDeviceInfo,
                                     aPresentationId,
                                     aControlChannel,
                                     aIsFromReceiver);
    this.releaseControlChannel(aControlChannel);
  },

  onSessionReconnect: function(aDeviceInfo, aUrl, aPresentationId, aControlChannel) {
    DEBUG && log("TCPPresentationServer - onSessionReconnect: " +
                 aDeviceInfo.address + ":" + aDeviceInfo.port); // jshint ignore:line
    if (!this.listener) {
      this.releaseControlChannel(aControlChannel);
      return;
    }

    this.listener.onReconnectRequest(aDeviceInfo,
                                     aUrl,
                                     aPresentationId,
                                     aControlChannel);
    this.releaseControlChannel(aControlChannel);
  },

  // nsIServerSocketListener (Triggered by nsIServerSocket.init)
  onSocketAccepted: function(aServerSocket, aClientSocket) {
    DEBUG && log("PresentationControlService - onSocketAccepted: " +
                 aClientSocket.host + ":" + aClientSocket.port); // jshint ignore:line
    let deviceInfo = new TCPDeviceInfo(aClientSocket.host, aClientSocket.port);
    this.holdControlChannel(this.responseSession(deviceInfo, aClientSocket));
  },

  holdControlChannel: function(aControlChannel) {
    this._controlChannels.push(aControlChannel);
  },

  releaseControlChannel: function(aControlChannel) {
    let index = this._controlChannels.indexOf(aControlChannel);
    if (index !== -1) {
      delete this._controlChannels[index];
    }
  },

  // nsIServerSocketListener (Triggered by nsIServerSocket.init)
  onStopListening: function(aServerSocket, aStatus) {
    DEBUG && log("PresentationControlService - onStopListening: " + aStatus); // jshint ignore:line
  },

  close: function() {
    DEBUG && log("PresentationControlService - close"); // jshint ignore:line
    if (this._isServiceInit()) {
      DEBUG && log("PresentationControlService - close server socket"); // jshint ignore:line
      this._serverSocket.close();
      this._serverSocket = null;

      Services.obs.removeObserver(this, "network-active-changed");
      Services.obs.removeObserver(this, "network:offline-status-changed");
    }
    this._port = 0;
  },

  // nsIObserver
  observe: function(aSubject, aTopic, aData) {
    DEBUG && log("PresentationControlService - observe: " + aTopic); // jshint ignore:line
    switch (aTopic) {
      case "network-active-changed": {
        if (!aSubject) {
          DEBUG && log("No active network"); // jshint ignore:line
          return;
        }

        /**
         * Restart service only when original status is online because other
         * cases will be handled by "network:offline-status-changed".
         */
        if (!Services.io.offline) {
          this._restartServer();
        }
        break;
      }
      case "network:offline-status-changed": {
        if (aData == "offline") {
          DEBUG && log("network offline"); // jshint ignore:line
          return;
        }
        this._restartServer();
        break;
      }
    }
  },

  _restartServer: function() {
    DEBUG && log("PresentationControlService - restart service"); // jshint ignore:line

    // restart server socket
    if (this._isServiceInit()) {
      let port = this._port;
      this.close();

      try {
        this.startServer();
        if (this._listener && this._port !== port) {
           this._listener.onPortChange(this._port);
        }
      } catch (e) {
        DEBUG && log("PresentationControlService - restart service fail: " + e); // jshint ignore:line
      }
    }
  },

  classID: Components.ID("{f4079b8b-ede5-4b90-a112-5b415a931deb}"),
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIServerSocketListener,
                                          Ci.nsIPresentationControlService,
                                          Ci.nsIObserver]),
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

  classID: Components.ID("{82507aea-78a2-487e-904a-858a6c5bf4e1}"),
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

function TCPControlChannel(presentationService,
                           transport,
                           deviceInfo,
                           direction) {
  DEBUG && log("create TCPControlChannel for : " + direction); // jshint ignore:line
  this._deviceInfo = deviceInfo;
  this._direction = direction;
  this._transport = transport;

  this._presentationService = presentationService;

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

  this._stateMachine =
    (direction === "sender") ? new ControllerStateMachine(this, presentationService.id)
                             : new ReceiverStateMachine(this);
  // Since the transport created by server socket is already CONNECTED_TO
  if (this._direction === "receiver") {
    this._createInputStreamPump();
  }
}

TCPControlChannel.prototype = {
  _connected: false,
  _pendingOpen: false,
  _pendingOffer: null,
  _pendingAnswer: null,
  _pendingClose: null,
  _pendingCloseReason: null,
  _pendingReconnect: false,

  sendOffer: function(aOffer) {
    this._stateMachine.sendOffer(discriptionAsJson(aOffer));
  },

  sendAnswer: function(aAnswer) {
    this._stateMachine.sendAnswer(discriptionAsJson(aAnswer));
  },

  sendIceCandidate: function(aCandidate) {
    this._stateMachine.updateIceCandidate(aCandidate);
  },

  launch: function(aPresentationId, aUrl) {
    this._stateMachine.launch(aPresentationId, aUrl);
  },

  terminate: function(aPresentationId) {
    if (!this._terminatingId) {
      this._terminatingId = aPresentationId;
      this._stateMachine.terminate(aPresentationId);
    } else {
      this._stateMachine.terminateAck(aPresentationId);
      delete this._terminatingId;
    }
  },

  // may throw an exception
  _send: function(aMsg) {
    DEBUG && log("TCPControlChannel - Send: " + JSON.stringify(aMsg, null, 2)); // jshint ignore:line

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
      DEBUG && log("TCPControlChannel - Failed to send message: " + e.name); // jshint ignore:line
      throw e;
    }
  },

  // nsIAsyncInputStream (Triggered by nsIInputStream.asyncWait)
  // Only used for detecting connection refused
  onInputStreamReady: function(aStream) {
    try {
      aStream.available();
    } catch (e) {
      DEBUG && log("TCPControlChannel - onInputStreamReady error: " + e.name); // jshint ignore:line
      // NS_ERROR_CONNECTION_REFUSED
      this._listener.notifyDisconnected(e.result);
    }
  },

  // nsITransportEventSink (Triggered by nsISocketTransport.setEventSink)
  onTransportStatus: function(aTransport, aStatus) {
    DEBUG && log("TCPControlChannel - onTransportStatus: " + aStatus.toString(16) +
                 " with role: " + this._direction); // jshint ignore:line
    if (aStatus === Ci.nsISocketTransport.STATUS_CONNECTED_TO) {
      this._connected = true;

      if (!this._pump) {
        this._createInputStreamPump();
      }
    }
  },

  // nsIRequestObserver (Triggered by nsIInputStreamPump.asyncRead)
  onStartRequest: function() {
    DEBUG && log("TCPControlChannel - onStartRequest with role: " +
                 this._direction); // jshint ignore:line
  },

  // nsIRequestObserver (Triggered by nsIInputStreamPump.asyncRead)
  onStopRequest: function(aRequest, aContext, aStatus) {
    DEBUG && log("TCPControlChannel - onStopRequest: " + aStatus +
                 " with role: " + this._direction); // jshint ignore:line
    this._stateMachine.onChannelClosed(aStatus, true);
  },

  // nsIStreamListener (Triggered by nsIInputStreamPump.asyncRead)
  onDataAvailable: function(aRequest, aContext, aInputStream) {
    let data = NetUtil.readInputStreamToString(aInputStream,
                                               aInputStream.available());
    DEBUG && log("TCPControlChannel - onDataAvailable: " + data); // jshint ignore:line

    // Parser of line delimited JSON. Please see |_send| for more informaiton.
    let jsonArray = data.split("\n");
    jsonArray.pop();
    for (let json of jsonArray) {
      let msg;
      try {
        msg = JSON.parse(json);
      } catch (e) {
        DEBUG && log("TCPSignalingChannel - error in parsing json: " + e); // jshint ignore:line
      }

      this._handleMessage(msg);
    }
  },

  _createInputStreamPump: function() {
    DEBUG && log("TCPControlChannel - create pump with role: " +
                 this._direction); // jshint ignore:line
    this._pump = Cc["@mozilla.org/network/input-stream-pump;1"].
               createInstance(Ci.nsIInputStreamPump);
    this._pump.init(this._input, -1, -1, 0, 0, false);
    this._pump.asyncRead(this, null);
    this._stateMachine.onChannelReady();
  },

  // Handle command from remote side
  _handleMessage: function(aMsg) {
    DEBUG && log("TCPControlChannel - handleMessage from " +
                 JSON.stringify(this._deviceInfo) + ": " + JSON.stringify(aMsg)); // jshint ignore:line
    this._stateMachine.onCommand(aMsg);
  },

  get listener() {
    return this._listener;
  },

  set listener(aListener) {
    DEBUG && log("TCPControlChannel - set listener: " + aListener); // jshint ignore:line
    if (!aListener) {
      this._listener = null;
      return;
    }

    this._listener = aListener;
    if (this._pendingOpen) {
      this._pendingOpen = false;
      DEBUG && log("TCPControlChannel - notify pending opened"); // jshint ignore:line
      this._listener.notifyConnected();
    }

    if (this._pendingOffer) {
      let offer = this._pendingOffer;
      DEBUG && log("TCPControlChannel - notify pending offer: " +
                   JSON.stringify(offer)); // jshint ignore:line
      this._listener.onOffer(new ChannelDescription(offer));
      this._pendingOffer = null;
    }

    if (this._pendingAnswer) {
      let answer = this._pendingAnswer;
      DEBUG && log("TCPControlChannel - notify pending answer: " +
                   JSON.stringify(answer)); // jshint ignore:line
      this._listener.onAnswer(new ChannelDescription(answer));
      this._pendingAnswer = null;
    }

    if (this._pendingClose) {
      DEBUG && log("TCPControlChannel - notify pending closed"); // jshint ignore:line
      this._notifyDisconnected(this._pendingCloseReason);
      this._pendingClose = null;
    }

    if (this._pendingReconnect) {
      DEBUG && log("TCPControlChannel - notify pending reconnected"); // jshint ignore:line
      this._notifyReconnected();
      this._pendingReconnect = false;
    }
  },

  /**
   * These functions are designed to handle the interaction with listener
   * appropriately. |_FUNC| is to handle |this._listener.FUNC|.
   */
  _onOffer: function(aOffer) {
    if (!this._connected) {
      return;
    }
    if (!this._listener) {
      this._pendingOffer = aOffer;
      return;
    }
    DEBUG && log("TCPControlChannel - notify offer: " +
                 JSON.stringify(aOffer)); // jshint ignore:line
    this._listener.onOffer(new ChannelDescription(aOffer));
  },

  _onAnswer: function(aAnswer) {
    if (!this._connected) {
      return;
    }
    if (!this._listener) {
      this._pendingAnswer = aAnswer;
      return;
    }
    DEBUG && log("TCPControlChannel - notify answer: " +
                 JSON.stringify(aAnswer)); // jshint ignore:line
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

    DEBUG && log("TCPControlChannel - notify opened with role: " +
                 this._direction); // jshint ignore:line
    this._listener.notifyConnected();
  },

  _notifyDisconnected: function(aReason) {
    this._connected = false;
    this._pendingOpen = false;
    this._pendingOffer = null;
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

    DEBUG && log("TCPControlChannel - notify closed with role: " +
                 this._direction); // jshint ignore:line
    this._listener.notifyDisconnected(aReason);
  },

  _notifyReconnected: function() {
    if (!this._listener) {
      this._pendingReconnect = true;
      return;
    }

    DEBUG && log("TCPControlChannel - notify reconnected with role: " +
                 this._direction); // jshint ignore:line
    this._listener.notifyReconnected();
  },

  _closeTransport: function() {
    if (this._connected) {
      this._transport.setEventSink(null, null);
      this._pump = null;

      this._input.close();
      this._output.close();
      this._presentationService.releaseControlChannel(this);
    }
  },

  disconnect: function(aReason) {
    DEBUG && log("TCPControlChannel - disconnect with reason: " + aReason); // jshint ignore:line

    if (this._connected) {
      // default reason is NS_OK
      aReason = !aReason ? Cr.NS_OK : aReason;
      this._stateMachine.onChannelClosed(aReason, false);

      this._closeTransport();

      this._connected = false;
    }
  },

  reconnect: function(aPresentationId, aUrl) {
    DEBUG && log("TCPControlChannel - reconnect with role: " +
                 this._direction); // jshint ignore:line
    if (this._direction != "sender") {
      return Cr.NS_ERROR_FAILURE;
    }

    this._stateMachine.reconnect(aPresentationId, aUrl);
  },

  // callback from state machine
  sendCommand: function(command) {
    this._send(command);
  },

  notifyDeviceConnected: function(deviceId) {
    switch (this._direction) {
      case "receiver":
        this._deviceInfo.id = deviceId;
        break;
    }
    this._notifyConnected();
  },

  notifyDisconnected: function(reason) {
    this._notifyDisconnected(reason);
    this._closeTransport();
    this._connected = false;
  },

  notifyLaunch: function(presentationId, url) {
    switch (this._direction) {
      case "receiver":
        this._presentationService.onSessionRequest(this._deviceInfo,
                                                   url,
                                                   presentationId,
                                                   this);
      break;
    }
  },

  notifyTerminate: function(presentationId) {
    if (!this._terminatingId) {
      this._terminatingId = presentationId;
      this._presentationService.onSessionTerminate(this._deviceInfo,
                                                   presentationId,
                                                   this,
                                                   this._direction === "sender");
      return;
    }

    if (this._terminatingId !== presentationId) {
      // Requested presentation Id doesn't matched with the one in ACK.
      // Disconnect the control channel with error.
      DEBUG && log("TCPControlChannel - unmatched terminatingId: " + presentationId); // jshint ignore:line
      this.disconnect(Cr.NS_ERROR_FAILURE);
    }

    delete this._terminatingId;
  },

  notifyReconnect: function(presentationId, url) {
    switch (this._direction) {
      case "receiver":
        this._presentationService.onSessionReconnect(this._deviceInfo,
                                                     url,
                                                     presentationId,
                                                     this);
        break;
      case "sender":
        this._notifyReconnected();
        break;
    }
  },

  notifyOffer: function(offer) {
    this._onOffer(offer);
  },

  notifyAnswer: function(answer) {
    this._onAnswer(answer);
  },

  notifyIceCandidate: function(candidate) {
    this._listener.onIceCandidate(candidate);
  },

  classID: Components.ID("{fefb8286-0bdc-488b-98bf-0c11b485c955}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationControlChannel,
                                         Ci.nsIStreamListener]),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PresentationControlService]); // jshint ignore:line
