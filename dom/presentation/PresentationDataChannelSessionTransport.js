/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// Bug 1228209 - plan to remove this eventually
function log(aMsg) {
  //dump("-*- PresentationDataChannelSessionTransport.js : " + aMsg + "\n");
}

const PRESENTATIONTRANSPORT_CID = Components.ID("{dd2bbf2f-3399-4389-8f5f-d382afb8b2d6}");
const PRESENTATIONTRANSPORT_CONTRACTID = "mozilla.org/presentation/datachanneltransport;1";

const PRESENTATIONTRANSPORTBUILDER_CID = Components.ID("{215b2f62-46e2-4004-a3d1-6858e56c20f3}");
const PRESENTATIONTRANSPORTBUILDER_CONTRACTID = "mozilla.org/presentation/datachanneltransportbuilder;1";

function PresentationDataChannelDescription(aDataChannelSDP) {
  this._dataChannelSDP = JSON.stringify(aDataChannelSDP);
}

PresentationDataChannelDescription.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationChannelDescription]),
  get type() {
    return nsIPresentationChannelDescription.TYPE_DATACHANNEL;
  },
  get tcpAddress() {
    return null;
  },
  get tcpPort() {
    return null;
  },
  get dataChannelSDP() {
    return this._dataChannelSDP;
  }
};

function PresentationTransportBuilder() {
  log("PresentationTransportBuilder construct");
  this._isControlChannelNeeded = true;
}

PresentationTransportBuilder.prototype = {
  classID: PRESENTATIONTRANSPORTBUILDER_CID,
  contractID: PRESENTATIONTRANSPORTBUILDER_CONTRACTID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationSessionTransportBuilder,
                                         Ci.nsIPresentationDataChannelSessionTransportBuilder,
                                         Ci.nsITimerCallback]),

  buildDataChannelTransport: function(aRole, aWindow, aListener) {
    if (!aRole || !aWindow || !aListener) {
      log("buildDataChannelTransport with illegal parameters");
      throw Cr.NS_ERROR_ILLEGAL_VALUE;
    }

    if (this._window) {
      log("buildDataChannelTransport has started.");
      throw Cr.NS_ERROR_UNEXPECTED;
    }

    log("buildDataChannelTransport with role " + aRole);
    this._role = aRole;
    this._window = aWindow;
    this._listener = aListener.QueryInterface(Ci.nsIPresentationSessionTransportBuilderListener);

    // TODO bug 1227053 set iceServers from |nsIPresentationDevice|
    this._peerConnection = new this._window.RTCPeerConnection();

    // |this._listener == null| will throw since the control channel is
    // abnormally closed.
    this._peerConnection.onicecandidate = aEvent => aEvent.candidate &&
      this._listener.sendIceCandidate(JSON.stringify(aEvent.candidate));

    this._peerConnection.onnegotiationneeded = () => {
      log("onnegotiationneeded with role " + this._role);
      this._peerConnection.createOffer()
          .then(aOffer => this._peerConnection.setLocalDescription(aOffer))
          .then(() => this._listener
                          .sendOffer(new PresentationDataChannelDescription(this._peerConnection.localDescription)))
          .catch(e => this._reportError(e));
    }

    switch (this._role) {
      case Ci.nsIPresentationService.ROLE_CONTROLLER:
        this._dataChannel = this._peerConnection.createDataChannel("presentationAPI");
        this._setDataChannel();
        break;

      case Ci.nsIPresentationService.ROLE_RECEIVER:
        this._peerConnection.ondatachannel = aEvent => {
          this._dataChannel = aEvent.channel;
          this._setDataChannel();
        }
        break;
      default:
       throw Cr.NS_ERROR_ILLEGAL_VALUE;
    }

    // TODO bug 1228235 we should have a way to let device providers customize
    // the time-out duration.
    let timeout;
    try {
      timeout = Services.prefs.getIntPref("presentation.receiver.loading.timeout");
    } catch (e) {
      // This happens if the pref doesn't exist, so we have a default value.
      timeout = 10000;
    }

    // The timer is to check if the negotiation finishes on time.
    this._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this._timer.initWithCallback(this, timeout, this._timer.TYPE_ONE_SHOT);
  },

  notify: function() {
    if (!this._sessionTransport) {
      this._cleanup(Cr.NS_ERROR_NET_TIMEOUT);
    }
  },

  _reportError: function(aError) {
    log("report Error " + aError.name + ":" + aError.message);
    this._cleanup(Cr.NS_ERROR_FAILURE);
  },

  _setDataChannel: function() {
    this._dataChannel.onopen = () => {
      log("data channel is open, notify the listener, role " + this._role);

      // Handoff the ownership of _peerConnection and _dataChannel to
      // _sessionTransport
      this._sessionTransport = new PresentationTransport();
      this._sessionTransport.init(this._peerConnection, this._dataChannel);
      this._peerConnection = this._dataChannel = null;

      this._listener.onSessionTransport(this._sessionTransport);
      this._sessionTransport.callback.notifyTransportReady();

      this._cleanup(Cr.NS_OK);
    };

    this._dataChannel.onerror = aError => {
      log("data channel onerror " + aError.name + ":" + aError.message);
      this._cleanup(Cr.NS_ERROR_FAILURE);
    }
  },

  _cleanup: function(aReason) {
    if (aReason != Cr.NS_OK) {
      this._listener.onError(aReason);
    }

    if (this._dataChannel) {
      this._dataChannel.close();
      this._dataChannel = null;
    }

    if (this._peerConnection) {
      this._peerConnection.close();
      this._peerConnection = null;
    }

    this._role = null;
    this._window = null;

    this._listener = null;
    this._sessionTransport = null;

    if (this._timer) {
      this._timer.cancel();
      this._timer = null;
    }
  },

  // nsIPresentationControlChannelListener
  onOffer: function(aOffer) {
    if (this._role !== Ci.nsIPresentationService.ROLE_RECEIVER ||
          this._sessionTransport) {
      log("onOffer status error");
      this._cleanup(Cr.NS_ERROR_FAILURE);
    }

    log("onOffer: " + aOffer.dataChannelSDP + " with role " + this._role);

    let offer = new this._window
                        .RTCSessionDescription(JSON.parse(aOffer.dataChannelSDP));

    this._peerConnection.setRemoteDescription(offer)
        .then(() => this._peerConnection.signalingState == "stable" ||
                      this._peerConnection.createAnswer())
        .then(aAnswer => this._peerConnection.setLocalDescription(aAnswer))
        .then(() => {
          this._isControlChannelNeeded = false;
          this._listener
              .sendAnswer(new PresentationDataChannelDescription(this._peerConnection.localDescription))
        }).catch(e => this._reportError(e));
  },

  onAnswer: function(aAnswer) {
    if (this._role !== Ci.nsIPresentationService.ROLE_CONTROLLER ||
          this._sessionTransport) {
      log("onAnswer status error");
      this._cleanup(Cr.NS_ERROR_FAILURE);
    }

    log("onAnswer: " + aAnswer.dataChannelSDP + " with role " + this._role);

    let answer = new this._window
                         .RTCSessionDescription(JSON.parse(aAnswer.dataChannelSDP));

    this._peerConnection.setRemoteDescription(answer).catch(e => this._reportError(e));
    this._isControlChannelNeeded = false;
  },

  onIceCandidate: function(aCandidate) {
    log("onIceCandidate: " + aCandidate + " with role " + this._role);
    let candidate = new this._window.RTCIceCandidate(JSON.parse(aCandidate));
    this._peerConnection.addIceCandidate(candidate).catch(e => this._reportError(e));
  },

  notifyDisconnected: function(aReason) {
    log("notifyDisconnected reason: " + aReason);

    if (aReason != Cr.NS_OK) {
      this._cleanup(aReason);
    } else if (this._isControlChannelNeeded) {
      this._cleanup(Cr.NS_ERROR_FAILURE);
    }
  },
};

function PresentationTransport() {
  this._messageQueue = [];
  this._closeReason = Cr.NS_OK;
}

PresentationTransport.prototype = {
  classID: PRESENTATIONTRANSPORT_CID,
  contractID: PRESENTATIONTRANSPORT_CONTRACTID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationSessionTransport]),

  init: function(aPeerConnection, aDataChannel) {
    log("initWithDataChannel");
    this._enableDataNotification = false;
    this._dataChannel = aDataChannel;
    this._peerConnection = aPeerConnection;

    this._dataChannel.onopen = () => {
      log("data channel reopen. Should never touch here");
    };

    this._dataChannel.onclose = () => {
      log("data channel onclose");
      if (this._callback) {
        this._callback.notifyTransportClosed(this._closeReason);
      }
      this._cleanup();
    }

    this._dataChannel.onmessage = aEvent => {
      log("data channel onmessage " + aEvent.data);

      if (!this._enableDataNotification || !this._callback) {
        log("queue message");
        this._messageQueue.push(aEvent.data);
        return;
      }
      this._callback.notifyData(aEvent.data);
    };

    this._dataChannel.onerror = aError => {
      log("data channel onerror " + aError.name + ":" + aError.message);
      if (this._callback) {
        this._callback.notifyTransportClosed(Cr.NS_ERROR_FAILURE);
      }
      this._cleanup();
    }
  },

  // nsIPresentationTransport
  get selfAddress() {
    throw NS_ERROR_NOT_AVAILABLE;
  },

  get callback() {
    return this._callback;
  },

  set callback(aCallback) {
    this._callback = aCallback;
  },

  send: function(aData) {
    log("send " + aData);
    this._dataChannel.send(aData);
  },

  enableDataNotification: function() {
    log("enableDataNotification");
    if (this._enableDataNotification) {
      return;
    }

    if (!this._callback) {
      throw NS_ERROR_NOT_AVAILABLE;
    }

    this._enableDataNotification = true;

    this._messageQueue.forEach(aData => this._callback.notifyData(aData));
    this._messageQueue = [];
  },

  close: function(aReason) {
    this._closeReason = aReason;

    this._dataChannel.close();
  },

  _cleanup: function() {
    this._dataChannel = null;

    if (this._peerConnection) {
      this._peerConnection.close();
      this._peerConnection = null;
    }
    this._callback = null;
    this._messageQueue = [];
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PresentationTransportBuilder,
                                                     PresentationTransport]);
