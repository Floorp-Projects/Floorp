/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

const DEBUG = Services.prefs.getBoolPref("dom.presentation.tcp_server.debug");
function log(aMsg) {
  dump("-*- TCPPresentationServer.js: " + aMsg + "\n");
}

function TCPDeviceInfo(aHost, aPort, aId, aName, aType) {
  this.host = aHost;
  this.port = aPort;
  this.id = aId;
  this.name = aName;
  this.type = aType;
}

function TCPPresentationServer() {
  this._id = null;
  this._port = 0;
  this._serverSocket = null;
  this._devices = new Map(); // id -> device
}

TCPPresentationServer.prototype = {
  /**
   * If a user agent connects to this server, we create a control channel but
   * hand it to |TCPDevice.listener| when the initial information exchange
   * finishes. Therefore, we hold the control channels in this period.
   */
  _controlChannels: [],

  init: function(aId, aPort) {
    if (this._isInit()) {
      DEBUG && log("TCPPresentationServer - server socket has been initialized");
      throw Cr.NS_ERROR_FAILURE;
    }

    if (typeof aId === "undefined" || typeof aPort === "undefined") {
      DEBUG && log("TCPPresentationServer - aId/aPort should not be undefined");
      throw Cr.NS_ERROR_FAILURE;
    }

    DEBUG && log("TCPPresentationServer - init id: " + aId + " port: " + aPort);

    /**
     * 0 or undefined indicates opt-out parameter, and a port will be selected
     * automatically.
     */
    let serverSocketPort = (aPort !== 0) ? aPort : -1;

    this._serverSocket = Cc["@mozilla.org/network/server-socket;1"]
                         .createInstance(Ci.nsIServerSocket);

    if (!this._serverSocket) {
      DEBUG && log("TCPPresentationServer - create server socket fail.");
      throw Cr.NS_ERROR_FAILURE;
    }

    try {
      this._serverSocket.init(serverSocketPort, false, -1);
    } catch (e) {
      // NS_ERROR_SOCKET_ADDRESS_IN_USE
      DEBUG && log("TCPPresentationServer - init server socket fail: " + e);
      throw Cr.NS_ERROR_FAILURE;
    }

    /**
     * The setter may trigger |_serverSocket.asyncListen| if the |id| setting
     * successes.
     */
    this.id = aId;
    this._port = this._serverSocket.port;
  },

  get id() {
    return this._id;
  },

  set id(aId) {
    if (!aId || aId.length == 0 || aId === this._id) {
      return;
    } else if (this._id) {
      throw Cr.NS_ERROR_FAILURE;
    }
    this._id = aId;

    if (this._serverSocket) {
      this._serverSocket.asyncListen(this);
    }
  },

  get port() {
    return this._port;
  },

  set listener(aListener) {
    this._listener = aListener;
  },

  get listener() {
    return this._listener;
  },

  _isInit: function() {
    return this._id !== null && this._serverSocket !== null;
  },

  close: function() {
    DEBUG && log("TCPPresentationServer - close");
    if (this._serverSocket) {
      this._serverSocket.close();
    }
    this._id = null;
    this._port = 0;
  },

  createTCPDevice: function(aId, aName, aType, aHost, aPort) {
    if (this._devices.has(aId)) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    this._devices.set(aId, new TCPDevice(this, {id: aId,
                                                name: aName,
                                                type: aType,
                                                host: aHost,
                                                port: aPort}));
    return this._devices.get(aId);
  },

  getTCPDevice: function(aId) {
    if (!this._devices.has(aId)) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }
    return this._devices.get(aId);
  },

  removeTCPDevice: function(aId) {
    if (!this._devices.has(aId)) {
      throw Cr.NS_ERROR_INVALID_ARG;
    }
    this._devices.delete(aId);
  },

  requestSession: function(aDevice, aUrl, aPresentationId) {
    if (!this._isInit()) {
      DEBUG && log("TCPPresentationServer - has not initialized; requestSession fails");
      return null;
    }
    DEBUG && log("TCPPresentationServer - requestSession to " + aDevice.name
                 + ": " + aUrl + ", " + aPresentationId);

    let sts = Cc["@mozilla.org/network/socket-transport-service;1"]
                .getService(Ci.nsISocketTransportService)

    let socketTransport;
    try {
      socketTransport = sts.createTransport(null,
                                            0,
                                            aDevice.host,
                                            aDevice.port,
                                            null);
    } catch (e) {
      DEBUG && log("TCPPresentationServer - createTransport throws: " + e);
      // Pop the exception to |TCPDevice.establishControlChannel|
      throw Cr.NS_ERROR_FAILURE;
    }
    return new TCPControlChannel(this,
                                 socketTransport,
                                 aDevice,
                                 aPresentationId,
                                 "sender",
                                 aUrl);
  },

  responseSession: function(aDevice, aSocketTransport) {
    if (!this._isInit()) {
      DEBUG && log("TCPPresentationServer - has not initialized; responseSession fails");
      return null;
    }
    DEBUG && log("TCPPresentationServer - responseSession to "
                 + JSON.stringify(aDevice));
    return new TCPControlChannel(this,
                                 aSocketTransport,
                                 aDevice,
                                 null, // presentation ID
                                 "receiver",
                                 null // url
                                 );
  },

  // Triggered by TCPControlChannel
  onSessionRequest: function(aId, aUrl, aPresentationId, aControlChannel) {
    let device = this._devices.get(aId);
    if (!device) {
      //XXX Bug 1136565 - should have a way to recovery
      DEBUG && log("TCPPresentationServer - onSessionRequest not found device for id: "
                   + aId );
      return;
    }
    device.listener.onSessionRequest(device,
                                     aUrl,
                                     aPresentationId,
                                     aControlChannel);
    this.releaseControlChannel(aControlChannel);
  },

  // nsIServerSocketListener (Triggered by nsIServerSocket.init)
  onSocketAccepted: function(aServerSocket, aClientSocket) {
    DEBUG && log("TCPPresentationServer - onSocketAccepted: "
                 + aClientSocket.host + ":" + aClientSocket.port);
    let device = new TCPDeviceInfo(aClientSocket.host, aClientSocket.port);
    this.holdControlChannel(this.responseSession(device, aClientSocket));
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
    DEBUG && log("TCPPresentationServer - onStopListening: " + aStatus);

    // manually closed
    if (aStatus === Cr.NS_BINDING_ABORTED) {
      aStatus = Cr.NS_OK;
    }
    this._listener && this._listener.onClose(aStatus);
    this._serverSocket = null;
  },

  close: function() {
    DEBUG && log("TCPPresentationServer - close signalling channel");
    if (this._serverSocket) {
      this._serverSocket.close();
      this._serverSocket = null;
    }
    this._id = null;
    this._port = 0;
    this._devices && this._devices.clear();
    this._devices = null;
  },

  classID: Components.ID("{f4079b8b-ede5-4b90-a112-5b415a931deb}"),
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIServerSocketListener,
                                          Ci.nsITCPPresentationServer]),
};

function ChannelDescription(aInit) {
  this._type = aInit.type;
  switch (this._type) {
    case Ci.nsIPresentationChannelDescription.TYPE_TCP:
      this._tcpAddresses = Cc["@mozilla.org/array;1"]
                           .createInstance(Ci.nsIMutableArray);
      for (let address of aInit.tcpAddress) {
        let wrapper = Cc["@mozilla.org/supports-string;1"]
                      .createInstance(Ci.nsISupportsString);
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
        let address = addresses.queryElementAt(idx, Ci.nsISupportsString);
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

function TCPControlChannel(presentationServer,
                           transport,
                           device,
                           presentationId,
                           direction,
                           url) {
  DEBUG && log("create TCPControlChannel: " + presentationId + " with role: "
               + direction);
  this._device = device;
  this._presentationId = presentationId;
  this._direction = direction;
  this._transport = transport;
  this._url = url;

  this._presentationServer =  presentationServer;

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
  _sendingMessageType: null,

  _sendMessage: function(aType, aJSONData, aOnThrow) {
    if (!aOnThrow) {
      aOnThrow = function(e) {throw e.result;}
    }

    if (!aType || !aJSONData) {
      aOnThrow();
      return;
    }

    if (!this._connected) {
      DEBUG && log("TCPControlChannel - send" + aType + " fails");
      throw Cr.NS_ERROR_FAILURE;
    }

    DEBUG && log("TCPControlChannel - send" + aType + ": "
                 + JSON.stringify(aJSONData));
    try {
      this._send(aJSONData);
    } catch (e) {
      aOnThrow(e);
    }
    this._sendingMessageType = aType;
  },

  _sendInit: function() {
    let msg = {
      type: "requestSession:Init",
      presentationId: this._presentationId,
      url: this._url,
      id: this._presentationServer.id,
    };

    this._sendMessage("init", msg, function(e) {
      this.close();
      this._notifyClosed(e.result);
    });
  },

  sendOffer: function(aOffer) {
    let msg = {
      type: "requestSession:Offer",
      presentationId: this.presentationId,
      offer: discriptionAsJson(aOffer),
    };
    this._sendMessage("offer", msg);
  },

  sendAnswer: function(aAnswer) {
    let msg = {
      type: "requestSession:Answer",
      presentationId: this.presentationId,
      answer: discriptionAsJson(aAnswer),
    };
    this._sendMessage("answer", msg);
  },

  // may throw an exception
  _send: function(aMsg) {
    DEBUG && log("TCPControlChannel - Send: " + JSON.stringify(aMsg, null, 2));

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
      DEBUG && log("TCPControlChannel - Failed to send message: " + e.name);
      throw e;
    }
  },

  // nsIAsyncInputStream (Triggered by nsIInputStream.asyncWait)
  // Only used for detecting connection refused
  onInputStreamReady: function(aStream) {
    try {
      aStream.available();
    } catch (e) {
      DEBUG && log("TCPControlChannel - onInputStreamReady error: " + e.name);
      // NS_ERROR_CONNECTION_REFUSED
      this._listener.notifyClosed(e.result);
    }
  },

  // nsITransportEventSink (Triggered by nsISocketTransport.setEventSink)
  onTransportStatus: function(aTransport, aStatus, aProg, aProgMax) {
    DEBUG && log("TCPControlChannel - onTransportStatus: "
                 + aStatus.toString(16) + " with role: " + this._direction);
    if (aStatus === Ci.nsISocketTransport.STATUS_CONNECTED_TO) {
      this._connected = true;

      if (!this._pump) {
        this._createInputStreamPump();
      }

      if (this._direction === "sender") {
        this._sendInit();
      }
    } else if (aStatus === Ci.nsISocketTransport.STATUS_SENDING_TO) {
      if (this._sendingMessageType === "init") {
        this._notifyOpened();
      }
      this._sendingMessageType = null;
    }
  },

  // nsIRequestObserver (Triggered by nsIInputStreamPump.asyncRead)
  onStartRequest: function() {
    DEBUG && log("TCPControlChannel - onStartRequest with role: "
                 + this._direction);
  },

  // nsIRequestObserver (Triggered by nsIInputStreamPump.asyncRead)
  onStopRequest: function(aRequest, aContext, aStatus) {
    DEBUG && log("TCPControlChannel - onStopRequest: " + aStatus
                 + " with role: " + this._direction);
    this.close();
    this._notifyClosed(aStatus);
  },

  // nsIStreamListener (Triggered by nsIInputStreamPump.asyncRead)
  onDataAvailable: function(aRequest, aContext, aInputStream, aOffset, aCount) {
    let data = NetUtil.readInputStreamToString(aInputStream,
                                               aInputStream.available());
    DEBUG && log("TCPControlChannel - onDataAvailable: " + data);

    // Parser of line delimited JSON. Please see |_send| for more informaiton.
    let jsonArray = data.split("\n");
    jsonArray.pop();
    for (let json of jsonArray) {
      let msg;
      try {
        msg = JSON.parse(json);
      } catch (e) {
        DEBUG && log("TCPSignalingChannel - error in parsing json: " + e);
      }

      this._handleMessage(msg);
    }
  },

  _createInputStreamPump: function() {
    DEBUG && log("TCPControlChannel - create pump with role: "
                 + this._direction);
    this._pump = Cc["@mozilla.org/network/input-stream-pump;1"].
               createInstance(Ci.nsIInputStreamPump);
    this._pump.init(this._input, -1, -1, 0, 0, false);
    this._pump.asyncRead(this, null);
  },

  // Handle command from remote side
  _handleMessage: function(aMsg) {
    DEBUG && log("TCPControlChannel - handleMessage from "
                 + JSON.stringify(this._device) + ": " + JSON.stringify(aMsg));
    switch (aMsg.type) {
      case "requestSession:Init": {
        this._device.id = aMsg.id;
        this._url = aMsg.url;
        this._presentationId = aMsg.presentationId;
        this._presentationServer.onSessionRequest(aMsg.id,
                                                  aMsg.url,
                                                  aMsg.presentationId,
                                                  this);
        this._notifyOpened();
        break;
      }
      case "requestSession:Offer": {
        this._listener.onOffer(new ChannelDescription(aMsg.offer));
        break;
      }
      case "requestSession:Answer": {
        this._listener.onAnswer(new ChannelDescription(aMsg.answer));
        break;
      }
    }
  },

  get listener() {
    return this._listener;
  },

  set listener(aListener) {
    DEBUG && log("TCPControlChannel - set listener: " + aListener);
    if (!aListener) {
      this._listener = null;
      return;
    }

    this._listener = aListener;
    if (this._pendingOpen) {
      this._pendingOpen = false;
      DEBUG && log("TCPControlChannel - notify pending opened");
      this._listener.notifyOpened();
    }

    if (this._pendingOffer) {
      let offer = this._pendingOffer;
      DEBUG && log("TCPControlChannel - notify pending offer: "
                   + JSON.stringify(offer));
      this._listener._onOffer(new ChannelDescription(offer));
      this._pendingOffer = null;
    }

    if (this._pendingAnswer) {
      let answer = this._pendingAnswer;
      DEBUG && log("TCPControlChannel - notify pending answer: "
                   + JSON.stringify(answer));
      this._listener._onAnswer(new ChannelDescription(answer));
      this._pendingAnswer = null;
    }

    if (this._pendingClose) {
      DEBUG && log("TCPControlChannel - notify pending closed");
      this._notifyClosed(this._pendingCloseReason);
      this._pendingClose = null;
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
      this._pendingOffer = offer;
      return;
    }
    DEBUG && log("TCPControlChannel - notify offer: "
                 + JSON.stringify(aOffer));
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
    DEBUG && log("TCPControlChannel - notify answer: "
                 + JSON.stringify(aAnswer));
    this._listener.onAnswer(new ChannelDescription(aAnswer));
  },

  _notifyOpened: function() {
    this._connected = true;
    this._pendingClose = false;
    this._pendingCloseReason = null;

    if (!this._listener) {
      this._pendingOpen = true;
      return;
    }

    DEBUG && log("TCPControlChannel - notify opened with role: "
                 + this._direction);
    this._listener.notifyOpened();
  },

  _notifyClosed: function(aReason) {
    this._connected = false;
    this._pendingOpen = false;
    this._pendingOffer = null;
    this._pendingAnswer = null;

    if (!this._listener) {
     this._pendingClose = true;
     this._pendingCloseReason = aReason;
     return;
    }

    DEBUG && log("TCPControlChannel - notify closed with role: "
                 + this._direction);
    this._listener.notifyClosed(aReason);
  },

  close: function() {
    DEBUG && log("TCPControlChannel - close");
    if (this._connected) {
      this._transport.setEventSink(null, null);
      this._pump = null;

      this._input.close();
      this._output.close();
      this._presentationServer.releaseControlChannel(this);

      this._connected = false;
    }
  },

  classID: Components.ID("{fefb8286-0bdc-488b-98bf-0c11b485c955}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationControlChannel,
                                         Ci.nsIStreamListener]),
};

function TCPDevice(aPresentationServer, aInfo) {
  DEBUG && log("create TCPDevice");
  this.id = aInfo.id;
  this.name = aInfo.name;
  this.type = aInfo.type
  this.host = aInfo.host;
  this.port = aInfo.port;

  this._presentationServer = aPresentationServer;
  this._listener = null;
}

TCPDevice.prototype = {
  establishControlChannel: function(aUrl, aPresentationId) {
    DEBUG && log("TCPDevice - establishControlChannel: " + aUrl + ", "
                 + aPresentationId);
    return this._presentationServer
               .requestSession(this._getDeviceInfo(), aUrl, aPresentationId);
  },
  get listener() {
    return this._listener;
  },
  set listener(aListener) {
    DEBUG && log("TCPDevice - set listener");
    this._listener = aListener;
  },

  _getDeviceInfo: function() {
    return new TCPDeviceInfo(this.host,
                             this.port,
                             this.id,
                             this.name,
                             this.type);
  },

  classID: Components.ID("{d6492549-a4f2-4a0c-9a93-00f0e9918b0a}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDevice]),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TCPPresentationServer]);
