/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const PC_CONTRACT = "@mozilla.org/dom/peerconnection;1";
const PC_ICE_CONTRACT = "@mozilla.org/dom/rtcicecandidate;1";
const PC_SESSION_CONTRACT = "@mozilla.org/dom/rtcsessiondescription;1";

const PC_CID = Components.ID("{7cb2b368-b1ce-4560-acac-8e0dbda7d3d0}");
const PC_ICE_CID = Components.ID("{8c5dbd70-2c8e-4ecb-a5ad-2fc919099f01}");
const PC_SESSION_CID = Components.ID("{5f21ffd9-b73f-4ba0-a685-56b4667aaf1c}");

// Global list of PeerConnection objects, so they can be cleaned up when
// a page is torn down. (Maps inner window ID to an array of PC objects).
function GlobalPCList() {
  this._list = {};
  Services.obs.addObserver(this, "inner-window-destroyed", true);
}
GlobalPCList.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  addPC: function(pc) {
    let winID = pc._winID;
    if (this._list[winID]) {
      this._list[winID].push(pc);
    } else {
      this._list[winID] = [pc];
    }
  },

  observe: function(subject, topic, data) {
    if (topic != "inner-window-destroyed") {
      return;
    }
    let winID = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
    if (this._list[winID]) {
      this._list[winID].forEach(function(pc) {
        pc._pc.close();
        delete pc._observer;
      });
      delete this._list[winID];
    }
  }
};
let _globalPCList = new GlobalPCList();

function IceCandidate(candidate) {
  this.candidate = candidate;
  this.sdpMid = null;
  this.sdpMLineIndex = null;
}
IceCandidate.prototype = {
  classID: PC_ICE_CID,

  classInfo: XPCOMUtils.generateCI({classID: PC_ICE_CID,
                                    contractID: PC_ICE_CONTRACT,
                                    classDescription: "IceCandidate",
                                    interfaces: [
                                      Ci.nsIDOMRTCIceCandidate
                                    ],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT}),

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIDOMRTCIceCandidate, Ci.nsIDOMGlobalObjectConstructor
  ]),

  constructor: function(win, cand, mid, mline) {
    if (this._win) {
      throw new Error("Constructor already called");
    }
    this._win = win;
    this.candidate = cand;
    this.sdpMid = mid;
    this.sdpMLineIndex = mline;
  }
};

function SessionDescription(type, sdp) {
  this.type = type;
  this.sdp = sdp;
}
SessionDescription.prototype = {
  classID: PC_SESSION_CID,

  classInfo: XPCOMUtils.generateCI({classID: PC_SESSION_CID,
                                    contractID: PC_SESSION_CONTRACT,
                                    classDescription: "SessionDescription",
                                    interfaces: [
                                      Ci.nsIDOMRTCSessionDescription
                                    ],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT}),

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIDOMRTCSessionDescription, Ci.nsIDOMGlobalObjectConstructor
  ]),

  constructor: function(win, type, sdp) {
    if (this._win) {
      throw new Error("Constructor already called");
    }
    this._win = win;
    this.type = type;
    this.sdp = sdp;
  },

  toString: function() {
    return JSON.stringify({
      type: this.type, sdp: this.sdp
    });
  }
};

function PeerConnection() {
  this._queue = [];

  this._pc = null;
  this._observer = null;

  this._onCreateOfferSuccess = null;
  this._onCreateOfferFailure = null;
  this._onCreateAnswerSuccess = null;
  this._onCreateAnswerFailure = null;

  /**
   * Everytime we get a request from content, we put it in the queue. If
   * there are no pending operations though, we will execute it immediately.
   * In PeerConnectionObserver, whenever we are notified that an operation
   * has finished, we will check the queue for the next operation and execute
   * if neccesary. The _pending flag indicates whether an operation is currently
   * in progress.
   */
  this._pending = false;

  // Public attributes.
  this.onaddstream = null;
  this.onremovestream = null;
  this.onicecandidate = null;
  this.onstatechange = null;
  this.ongatheringchange = null;
  this.onicechange = null;

  // Data channel.
  this.ondatachannel = null;
  this.onconnection = null;
  this.onclosedconnection = null;
}
PeerConnection.prototype = {
  classID: PC_CID,

  classInfo: XPCOMUtils.generateCI({classID: PC_CID,
                                    contractID: PC_CONTRACT,
                                    classDescription: "PeerConnection",
                                    interfaces: [
                                      Ci.nsIDOMRTCPeerConnection
                                    ],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT}),

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIDOMRTCPeerConnection, Ci.nsIDOMGlobalObjectConstructor
  ]),

  // Constructor is an explicit function, because of nsIDOMGlobalObjectConstructor.
  constructor: function(win) {
    if (!Services.prefs.getBoolPref("media.peerconnection.enabled")) {
      throw new Error("PeerConnection not enabled (did you set the pref?)");
    }
    if (this._win) {
      throw new Error("Constructor already called");
    }

    this._pc = Cc["@mozilla.org/peerconnection;1"].
             createInstance(Ci.IPeerConnection);
    this._observer = new PeerConnectionObserver(this);

    // Nothing starts until ICE gathering completes.
    this._queueOrRun({
      func: this._pc.initialize,
      args: [this._observer, win, Services.tm.currentThread],
      wait: true
    });

    this._win = win;
    this._winID = this._win.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;

    // Add a reference to the PeerConnection to global list.
    _globalPCList.addPC(this);
  },

  /**
   * Add a function to the queue or run it immediately if the queue is empty.
   * Argument is an object with the func, args and wait properties; wait should
   * be set to true if the function has a success/error callback that will
   * call _executeNext, false if it doesn't have a callback.
   */
  _queueOrRun: function(obj) {
    if (!this._pending) {
      obj.func.apply(this, obj.args);
      if (obj.wait) {
        this._pending = true;
      }
    } else {
      this._queue.push(obj);
    }
  },

  // Pick the next item from the queue and run it.
  _executeNext: function() {
    if (this._queue.length) {
      let obj = this._queue.shift();
      obj.func.apply(this, obj.args);
      if (!obj.wait) {
        this._executeNext();
      }
    } else {
      this._pending = false;
    }
  },

  createOffer: function(onSuccess, onError, constraints) {
    if (this._onCreateOfferSuccess) {
      if (onError) {
        onError.onCallback("createOffer already called");
      }
      return;
    }

    this._onCreateOfferSuccess = onSuccess;
    this._onCreateOfferFailure = onError;

    // TODO: Implement constraints/hints.
    if (!constraints) {
      constraints = "";
    }

    this._queueOrRun({
      func: this._pc.createOffer,
      args: [constraints],
      wait: true
    });
  },

  createAnswer: function(offer, onSuccess, onError, constraints, provisional) {
    if (this._onCreateAnswerSuccess) {
      if (onError) {
        onError.onCallback("createAnswer already called");
      }
      return;
    }

    this._onCreateAnswerSuccess = onSuccess;
    this._onCreateAnswerFailure = onError;

    if (offer.type != "offer") {
      if (onError) {
        onError.onCallback("Invalid type " + offer.type + " passed");
      }
      return;
    }

    if (!offer.sdp) {
      if (onError) {
        onError.onCallback("SDP not provided to createAnswer");
      }
      return;
    }

    if (!constraints) {
      constraints = "";
    }
    if (!provisional) {
      provisional = false;
    }

    // TODO: Implement provisional answer & constraints.
    this._queueOrRun({
      func: this._pc.createAnswer,
      args: ["", offer.sdp],
      wait: true
    });
  },

  setLocalDescription: function(desc, onSuccess, onError) {
    if (this._onSetLocalDescriptionSuccess) {
      if (onError) {
        onError.onCallback("setLocalDescription already called");
      }
      return;
    }

    this._onSetLocalDescriptionSuccess = onSuccess;
    this._onSetLocalDescriptionFailure = onError;

    let type;
    switch (desc.type) {
      case "offer":
        type = Ci.IPeerConnection.kActionOffer;
        break;
      case "answer":
        type = Ci.IPeerConnection.kActionAnswer;
        break;
      default:
        if (onError) {
          onError.onCallback(
            "Invalid type " + desc.type + " provided to setLocalDescription"
          );
          return;
        }
        break;
    }

    this._queueOrRun({
      func: this._pc.setLocalDescription,
      args: [type, desc.sdp],
      wait: true
    });
  },

  setRemoteDescription: function(desc, onSuccess, onError) {
    if (this._onSetRemoteDescriptionSuccess) {
      if (onError) {
        onError.onCallback("setRemoteDescription already called");
      }
      return;
    }

    this._onSetRemoteDescriptionSuccess = onSuccess;
    this._onSetRemoteDescriptionFailure = onError;

    let type;
    switch (desc.type) {
      case "offer":
        type = Ci.IPeerConnection.kActionOffer;
        break;
      case "answer":
        type = Ci.IPeerConnection.kActionAnswer;
        break;
      default:
        if (onError) {
          onError.onCallback(
            "Invalid type " + desc.type + " provided to setLocalDescription"
          );
          return;
        }
        break;
    }

    this._queueOrRun({
      func: this._pc.setRemoteDescription,
      args: [type, desc.sdp],
      wait: true
    });
  },

  updateIce: function(config, constraints, restart) {
    return Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  addIceCandidate: function(cand) {
    if (!cand) {
      throw "Invalid candidate passed to addIceCandidate!";
    }
    if (!cand.candidate || !cand.sdpMid || !cand.sdpMLineIndex) {
      throw "Invalid candidate passed to addIceCandidate!";
    }

    this._queueOrRun({
      func: this._pc.addIceCandidate,
      args: [cand.candidate, cand.sdpMid, cand.sdpMLineIndex],
      wait: false
    });
  },

  addStream: function(stream, constraints) {
    // TODO: Implement constraints.
    this._queueOrRun({
      func: this._pc.addStream,
      args: [stream],
      wait: false
    });
  },

  removeStream: function(stream) {
    this._queueOrRun({
      func: this._pc.removeStream,
      args: [stream],
      wait: false
    });
  },

  close: function() {
    this._queueOrRun({
      func: this._pc.close,
      args: [],
      wait: false
    });
  },

  createDataChannel: function(label, dict) {
    if (dict &&
        dict.maxRetransmitTime != undefined &&
        dict.maxRetransmitNum != undefined) {
      throw new Error("Both maxRetransmitTime and maxRetransmitNum cannot be provided");
    }

    // Must determine the type where we still know if entries are undefined.
    let type;
    if (dict.maxRetransmitTime != undefined) {
      type = Ci.IPeerConnection.DATACHANNEL_PARTIAL_RELIABLE_TIMED;
    } else if (dict.maxRetransmitNum != undefined) {
      type = Ci.IPeerConnection.DATACHANNEL_PARTIAL_RELIABLE_REXMIT;
    } else {
      type = Ci.IPeerConnection.DATACHANNEL_RELIABLE;
    }

    // Synchronous since it doesn't block.
    let channel = this._pc.createDataChannel(
      label, type, dict.outOfOrderAllowed, dict.maxRetransmitTime,
      dict.maxRetransmitNum
    );
    return channel;
  },

  connectDataConnection: function(localport, remoteport, numstreams) {
    if (numstreams == undefined || numstreams <= 0) {
      numstreams = 16;
    }
    this._queueOrRun({
      func: this._pc.connectDataConnection,
      args: [localport, remoteport, numstreams],
      wait: false
    });
  }
};

// This is a seperate object because we don't want to expose it to DOM.
function PeerConnectionObserver(dompc) {
  this._dompc = dompc;
}
PeerConnectionObserver.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.IPeerConnectionObserver]),

  onCreateOfferSuccess: function(offer) {
    if (this._dompc._onCreateOfferSuccess) {
      try {
        this._dompc._onCreateOfferSuccess.onCallback({
          type: "offer", sdp: offer,
          __exposedProps__: { type: "rw", sdp: "rw" }
        });
      } catch(e) {}
    }
    this._dompc._executeNext();
  },

  onCreateOfferError: function(code) {
    if (this._dompc._onCreateOfferFailure) {
      try {
        this._dompc._onCreateOfferFailure.onCallback(code);
      } catch(e) {}
    }
    this._dompc._executeNext();
  },

  onCreateAnswerSuccess: function(answer) {
    if (this._dompc._onCreateAnswerSuccess) {
      try {
        this._dompc._onCreateAnswerSuccess.onCallback({
          type: "answer", sdp: answer,
          __exposedProps__: { type: "rw", sdp: "rw" }
        });
      } catch(e) {}
    }
    this._dompc._executeNext();
  },

  onCreateAnswerError: function(code) {
    if (this._dompc._onCreateAnswerFailure) {
      try {
        this._dompc._onCreateAnswerFailure.onCallback(code);
      } catch(e) {}
    }
    this._dompc._executeNext();
  },

  onSetLocalDescriptionSuccess: function(code) {
    if (this._dompc._onSetLocalDescriptionSuccess) {
      try {
        this._dompc._onSetLocalDescriptionSuccess.onCallback(code);
      } catch(e) {}
    }
    this._dompc._executeNext();
  },

  onSetRemoteDescriptionSuccess: function(code) {
    if (this._dompc._onSetRemoteDescriptionSuccess) {
      try {
        this._dompc._onSetRemoteDescriptionSuccess.onCallback(code);
      } catch(e) {}
    }
    this._dompc._executeNext();
  },

  onSetLocalDescriptionError: function(code) {
    if (this._dompc._onSetLocalDescriptionFailure) {
      try {
        this._dompc._onSetLocalDescriptionFailure.onCallback(code);
      } catch(e) {}
    }
    this._dompc._executeNext();
  },

  onSetRemoteDescriptionError: function(code) {
    if (this._dompc._onSetRemoteDescriptionFailure) {
      this._dompc._onSetRemoteDescriptionFailure.onCallback(code);
    }
    this._dompc._executeNext();
  },

  onStateChange: function(state) {
    if (state != Ci.IPeerConnectionObserver.kIceState) {
      return;
    }

    let self = this;
    let iceCb = function() {};
    let iceGatherCb = function() {};
    if (this._dompc.onicechange) {
      iceCb = function(args) {
        try {
          self._dompc.onicechange(args);
        } catch(e) {}
      };
    }
    if (this._dompc.ongatheringchange) {
      iceGatherCb = function(args) {
        try {
          self._dompc.ongatheringchange(args);
        } catch(e) {}
      };
    }

    switch (this._dompc._pc.iceState) {
      case Ci.IPeerConnection.kIceGathering:
        iceGatherCb("gathering");
        break;
      case Ci.IPeerConnection.kIceWaiting:
        iceCb("starting");
        this._dompc._executeNext();
        break;
      case Ci.IPeerConnection.kIceChecking:
        iceCb("checking");
        this._dompc._executeNext();
        break;
      case Ci.IPeerConnection.kIceConnected:
        // ICE gathering complete.
        iceCb("connected");
        iceGatherCb("complete");
        this._dompc._executeNext();
        break;
      case Ci.IPeerConnection.kIceFailed:
        iceCb("failed");
        break;
      default:
        // Unknown state!
        break;
    }
  },

  onAddStream: function(stream, type) {
    if (this._dompc.onaddstream) {
      try {
        this._dompc.onaddstream.onCallback({
          stream: stream, type: type,
          __exposedProps__: { stream: "r", type: "r" }
        });
      } catch(e) {}
    }
    this._dompc._executeNext();
  },

  onRemoveStream: function(stream, type) {
    if (this._dompc.onremovestream) {
      try {
        this._dompc.onremovestream.onCallback({
          stream: stream, type: type,
          __exposedProps__: { stream: "r", type: "r" }
        });
      } catch(e) {}
    }
    this._dompc._executeNext();
  },

  foundIceCandidate: function(cand) {
    if (this._dompc.onicecandidate) {
      try {
        this._dompc.onicecandidate.onCallback({
          candidate: cand,
          __exposedProps__: { candidate: "rw" }
        });
      } catch(e) {}
    }
    this._dompc._executeNext();
  },

  notifyDataChannel: function(channel) {
    if (this._dompc.ondatachannel) {
      try {
        this._dompc.ondatachannel.onCallback(channel);
      } catch(e) {}
    }
    this._dompc._executeNext();
  },

  notifyConnection: function() {
    if (this._dompc.onconnection) {
      try {
        this._dompc.onconnection.onCallback();
      } catch(e) {}
    }
    this._dompc._executeNext();
  },

  notifyClosedConnection: function() {
    if (this._dompc.onclosedconnection) {
      try {
        this._dompc.onclosedconnection.onCallback();
      } catch(e) {}
    }
    this._dompc._executeNext();
  }
};

let NSGetFactory = XPCOMUtils.generateNSGetFactory(
  [IceCandidate, SessionDescription, PeerConnection]
);
