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
/* globals setTimeout, clearTimeout */
Cu.import("resource://gre/modules/Timer.jsm");

/* globals ControllerStateMachine */
XPCOMUtils.defineLazyModuleGetter(this, "ControllerStateMachine", // jshint ignore:line
                                  "resource://gre/modules/presentation/ControllerStateMachine.jsm");
/* global ReceiverStateMachine */
XPCOMUtils.defineLazyModuleGetter(this, "ReceiverStateMachine", // jshint ignore:line
                                  "resource://gre/modules/presentation/ReceiverStateMachine.jsm");

const kProtocolVersion = 1; // need to review isCompatibleServer while fiddling the version number.
const kLocalCertName = "presentation";

const DEBUG = Services.prefs.getBoolPref("dom.presentation.tcp_server.debug");
function log(aMsg) {
  dump("-*- PresentationControlService.js: " + aMsg + "\n");
}

function TCPDeviceInfo(aAddress, aPort, aId, aCertFingerprint) {
  this.address = aAddress;
  this.port = aPort;
  this.id = aId;
  this.certFingerprint = aCertFingerprint || "";
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

  startServer: function(aEncrypted, aPort) {
    if (this._isServiceInit()) {
      DEBUG && log("PresentationControlService - server socket has been initialized");  // jshint ignore:line
      throw Cr.NS_ERROR_FAILURE;
    }

    /**
     * 0 or undefined indicates opt-out parameter, and a port will be selected
     * automatically.
     */
    let serverSocketPort = (typeof aPort !== "undefined" && aPort !== 0) ? aPort : -1;

    if (aEncrypted) {
      let self = this;
      let localCertService = Cc["@mozilla.org/security/local-cert-service;1"]
                               .getService(Ci.nsILocalCertService);
      localCertService.getOrCreateCert(kLocalCertName, {
        handleCert: function(aCert, aRv) {
          DEBUG && log("PresentationControlService - handleCert");  // jshint ignore:line
          if (aRv) {
            self._notifyServerStopped(aRv);
          } else {
            self._serverSocket = Cc["@mozilla.org/network/tls-server-socket;1"]
                                   .createInstance(Ci.nsITLSServerSocket);

            self._serverSocketInit(serverSocketPort, aCert);
          }
        }
      });
    } else {
      this._serverSocket = Cc["@mozilla.org/network/server-socket;1"]
                             .createInstance(Ci.nsIServerSocket);

      this._serverSocketInit(serverSocketPort, null);
    }
  },

  _serverSocketInit: function(aPort, aCert) {
    if (!this._serverSocket) {
      DEBUG && log("PresentationControlService - create server socket fail."); // jshint ignore:line
      throw Cr.NS_ERROR_FAILURE;
    }

    try {
      this._serverSocket.init(aPort, false, -1);

      if (aCert) {
        this._serverSocket.serverCert = aCert;
        this._serverSocket.setSessionCache(false);
        this._serverSocket.setSessionTickets(false);
        let requestCert = Ci.nsITLSServerSocket.REQUEST_NEVER;
        this._serverSocket.setRequestClientCertificate(requestCert);
      }

      this._serverSocket.asyncListen(this);
    } catch (e) {
      // NS_ERROR_SOCKET_ADDRESS_IN_USE
      DEBUG && log("PresentationControlService - init server socket fail: " + e); // jshint ignore:line
      throw Cr.NS_ERROR_FAILURE;
    }

    this._port = this._serverSocket.port;

    DEBUG && log("PresentationControlService - service start on port: " + this._port); // jshint ignore:line

    // Monitor network interface change to restart server socket.
    Services.obs.addObserver(this, "network:offline-status-changed");

    this._notifyServerReady();
  },

  _notifyServerReady: function() {
    Services.tm.dispatchToMainThread(() => {
      if (this._listener) {
        this._listener.onServerReady(this._port, this.certFingerprint);
      }
    });
  },

  _notifyServerStopped: function(aRv) {
    Services.tm.dispatchToMainThread(() => {
      if (this._listener) {
        this._listener.onServerStopped(aRv);
      }
    });
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

  get certFingerprint() {
    if (!this._serverSocket.serverCert) {
      return null;
    }

    return this._serverSocket.serverCert.sha256Fingerprint;
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

    let socketTransport = this._attemptConnect(aDeviceInfo);
    return new TCPControlChannel(this,
                                 socketTransport,
                                 aDeviceInfo,
                                 "sender");
  },

  _attemptConnect: function(aDeviceInfo) {
    let sts = Cc["@mozilla.org/network/socket-transport-service;1"]
                .getService(Ci.nsISocketTransportService);

    let socketTransport;
    try {
      if (aDeviceInfo.certFingerprint) {
        let overrideService = Cc["@mozilla.org/security/certoverride;1"]
                                .getService(Ci.nsICertOverrideService);
        overrideService.rememberTemporaryValidityOverrideUsingFingerprint(
            aDeviceInfo.address,
            aDeviceInfo.port,
            aDeviceInfo.certFingerprint,
            Ci.nsICertOverrideService.ERROR_UNTRUSTED | Ci.nsICertOverrideService.ERROR_MISMATCH);

        socketTransport = sts.createTransport(["ssl"],
                                              1,
                                              aDeviceInfo.address,
                                              aDeviceInfo.port,
                                              null);
      } else {
        socketTransport = sts.createTransport(null,
                                              0,
                                              aDeviceInfo.address,
                                              aDeviceInfo.port,
                                              null);
      }
      // Shorten the connection failure procedure.
      socketTransport.setTimeout(Ci.nsISocketTransport.TIMEOUT_CONNECT, 2);
    } catch (e) {
      DEBUG && log("PresentationControlService - createTransport throws: " + e);  // jshint ignore:line
      // Pop the exception to |TCPDevice.establishControlChannel|
      throw Cr.NS_ERROR_FAILURE;
    }
    return socketTransport;
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

      Services.obs.removeObserver(this, "network:offline-status-changed");

      this._notifyServerStopped(Cr.NS_OK);
    }
    this._port = 0;
  },

  // nsIObserver
  observe: function(aSubject, aTopic, aData) {
    DEBUG && log("PresentationControlService - observe: " + aTopic); // jshint ignore:line
    switch (aTopic) {
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
      this.close();

      try {
        this.startServer();
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

const kDisconnectTimeout = 5000;
const kTerminateTimeout = 5000;

function TCPControlChannel(presentationService,
                           transport,
                           deviceInfo,
                           direction) {
  DEBUG && log("create TCPControlChannel for : " + direction); // jshint ignore:line
  this._deviceInfo = deviceInfo;
  this._direction = direction;
  this._transport = transport;

  this._presentationService = presentationService;

  if (direction === "receiver") {
    // Need to set security observer before I/O stream operation.
    this._setSecurityObserver(this);
  }

  let currentThread = Services.tm.currentThread;
  transport.setEventSink(this, currentThread);

  this._input = this._transport.openInputStream(0, 0, 0)
                               .QueryInterface(Ci.nsIAsyncInputStream);
  this._input.asyncWait(this.QueryInterface(Ci.nsIStreamListener),
                        Ci.nsIAsyncInputStream.WAIT_CLOSURE_ONLY,
                        0,
                        currentThread);

  this._output = this._transport
                     .openOutputStream(Ci.nsITransport.OPEN_UNBUFFERED, 0, 0)
                     .QueryInterface(Ci.nsIAsyncOutputStream);

  this._outgoingMsgs = [];


  this._stateMachine =
    (direction === "sender") ? new ControllerStateMachine(this, presentationService.id)
                             : new ReceiverStateMachine(this);

  if (direction === "receiver" && !transport.securityInfo) {
    // Since the transport created by server socket is already CONNECTED_TO.
    this._outgoingEnabled = true;
    this._createInputStreamPump();
  }
}

TCPControlChannel.prototype = {
  _outgoingEnabled: false,
  _incomingEnabled: false,
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

      // Start a guard timer to ensure terminateAck is processed.
      this._terminateTimer = setTimeout(() => {
        DEBUG && log("TCPControlChannel - terminate timeout: " + aPresentationId); // jshint ignore:line
        delete this._terminateTimer;
        if (this._pendingDisconnect) {
          this._pendingDisconnect();
        } else {
          this.disconnect(Cr.NS_OK);
        }
      }, kTerminateTimeout);
    } else {
      this._stateMachine.terminateAck(aPresentationId);
      delete this._terminatingId;
    }
  },

  _flushOutgoing: function() {
    if (!this._outgoingEnabled || this._outgoingMsgs.length === 0) {
      return;
    }

    this._output.asyncWait(this, 0, 0, Services.tm.currentThread);
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

  _setSecurityObserver: function(observer) {
    if (this._transport && this._transport.securityInfo) {
      DEBUG && log("TCPControlChannel - setSecurityObserver: " + observer); // jshint ignore:line
      let connectionInfo = this._transport.securityInfo
                               .QueryInterface(Ci.nsITLSServerConnectionInfo);
      connectionInfo.setSecurityObserver(observer);
    }
  },

  // nsITLSServerSecurityObserver
  onHandshakeDone: function(socket, clientStatus) {
    log("TCPControlChannel - onHandshakeDone: TLS version: " + clientStatus.tlsVersionUsed.toString(16));
    this._setSecurityObserver(null);

    // Process input/output after TLS handshake is complete.
    this._outgoingEnabled = true;
    this._createInputStreamPump();
  },

  // nsIAsyncOutputStream
  onOutputStreamReady: function() {
    DEBUG && log("TCPControlChannel - onOutputStreamReady"); // jshint ignore:line
    if (this._outgoingMsgs.length === 0) {
      return;
    }

    try {
      this._send(this._outgoingMsgs[0]);
    } catch (e) {
      if (e.result === Cr.NS_BASE_STREAM_WOULD_BLOCK) {
        this._output.asyncWait(this, 0, 0, Services.tm.currentThread);
        return;
      }

      this._closeTransport();
      return;
    }
    this._outgoingMsgs.shift();
    this._flushOutgoing();
  },

  // nsIAsyncInputStream (Triggered by nsIInputStream.asyncWait)
  // Only used for detecting connection refused
  onInputStreamReady: function(aStream) {
    DEBUG && log("TCPControlChannel - onInputStreamReady"); // jshint ignore:line
    try {
      aStream.available();
    } catch (e) {
      DEBUG && log("TCPControlChannel - onInputStreamReady error: " + e.name); // jshint ignore:line
      // NS_ERROR_CONNECTION_REFUSED
      this._notifyDisconnected(e.result);
    }
  },

  // nsITransportEventSink (Triggered by nsISocketTransport.setEventSink)
  onTransportStatus: function(aTransport, aStatus) {
    DEBUG && log("TCPControlChannel - onTransportStatus: " + aStatus.toString(16) +
                 " with role: " + this._direction); // jshint ignore:line
    if (aStatus === Ci.nsISocketTransport.STATUS_CONNECTED_TO) {
      this._outgoingEnabled = true;
      this._createInputStreamPump();
    }
  },

  // nsIRequestObserver (Triggered by nsIInputStreamPump.asyncRead)
  onStartRequest: function() {
    DEBUG && log("TCPControlChannel - onStartRequest with role: " +
                 this._direction); // jshint ignore:line
    this._incomingEnabled = true;
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
    if (this._pump) {
      return;
    }

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
    if (!this._incomingEnabled) {
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
    if (!this._incomingEnabled) {
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

  _closeOutgoing: function() {
    if (this._outgoingEnabled) {
      this._output.close();
      this._outgoingEnabled = false;
    }
  },
  _closeIncoming: function() {
    if (this._incomingEnabled) {
      this._pump = null;
      this._input.close();
      this._incomingEnabled = false;
    }
  },
  _closeTransport: function() {
    if (this._disconnectTimer) {
      clearTimeout(this._disconnectTimer);
      delete this._disconnectTimer;
    }

    if (this._terminateTimer) {
      clearTimeout(this._terminateTimer);
      delete this._terminateTimer;
    }

    delete this._pendingDisconnect;

    this._transport.setEventSink(null, null);

    this._closeIncoming();
    this._closeOutgoing();
    this._presentationService.releaseControlChannel(this);
  },

  disconnect: function(aReason) {
    DEBUG && log("TCPControlChannel - disconnect with reason: " + aReason); // jshint ignore:line

    // Pending disconnect during termination procedure.
    if (this._terminateTimer) {
      // Store only the first disconnect action.
      if (!this._pendingDisconnect) {
        this._pendingDisconnect = this.disconnect.bind(this, aReason);
      }
      return;
    }

    if (this._outgoingEnabled && !this._disconnectTimer) {
      // default reason is NS_OK
      aReason = !aReason ? Cr.NS_OK : aReason;

      this._stateMachine.onChannelClosed(aReason, false);

      // Start a guard timer to ensure the transport will be closed.
      this._disconnectTimer = setTimeout(() => {
        DEBUG && log("TCPControlChannel - disconnect timeout"); // jshint ignore:line
        this._closeTransport();
      }, kDisconnectTimeout);
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
    this._outgoingMsgs.push(command);
    this._flushOutgoing();
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
    this._closeTransport();
    this._notifyDisconnected(reason);
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

    // Cancel terminate guard timer after receiving terminate-ack.
    if (this._terminateTimer) {
      clearTimeout(this._terminateTimer);
      delete this._terminateTimer;
    }

    if (this._terminatingId !== presentationId) {
      // Requested presentation Id doesn't matched with the one in ACK.
      // Disconnect the control channel with error.
      DEBUG && log("TCPControlChannel - unmatched terminatingId: " + presentationId); // jshint ignore:line
      this.disconnect(Cr.NS_ERROR_FAILURE);
    }

    delete this._terminatingId;
    if (this._pendingDisconnect) {
      this._pendingDisconnect();
    }
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
