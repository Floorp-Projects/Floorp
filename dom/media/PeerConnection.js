/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const PC_CONTRACT = "@mozilla.org/dom/peerconnection;1";
const PC_ICE_CONTRACT = "@mozilla.org/dom/rtcicecandidate;1";
const PC_SESSION_CONTRACT = "@mozilla.org/dom/rtcsessiondescription;1";
const PC_MANAGER_CONTRACT = "@mozilla.org/dom/peerconnectionmanager;1";

const PC_CID = Components.ID("{7cb2b368-b1ce-4560-acac-8e0dbda7d3d0}");
const PC_ICE_CID = Components.ID("{8c5dbd70-2c8e-4ecb-a5ad-2fc919099f01}");
const PC_SESSION_CID = Components.ID("{5f21ffd9-b73f-4ba0-a685-56b4667aaf1c}");
const PC_MANAGER_CID = Components.ID("{7293e901-2be3-4c02-b4bd-cbef6fc24f78}");

// Global list of PeerConnection objects, so they can be cleaned up when
// a page is torn down. (Maps inner window ID to an array of PC objects).
function GlobalPCList() {
  this._list = [];
  this._networkdown = false; // XXX Need to query current state somehow
  Services.obs.addObserver(this, "inner-window-destroyed", true);
  Services.obs.addObserver(this, "profile-change-net-teardown", true);
  Services.obs.addObserver(this, "network:offline-about-to-go-offline", true);
  Services.obs.addObserver(this, "network:offline-status-changed", true);
}
GlobalPCList.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference,
                                         Ci.IPeerConnectionManager]),

  classID: PC_MANAGER_CID,
  classInfo: XPCOMUtils.generateCI({classID: PC_MANAGER_CID,
                                    contractID: PC_MANAGER_CONTRACT,
                                    classDescription: "PeerConnectionManager",
                                    interfaces: [
                                      Ci.nsIObserver,
                                      Ci.nsISupportsWeakReference,
                                      Ci.IPeerConnectionManager
                                    ]}),

  _xpcom_factory: {
    createInstance: function(outer, iid) {
      if (outer) {
        throw Cr.NS_ERROR_NO_AGGREGATION;
      }
      return _globalPCList.QueryInterface(iid);
    }
  },

  addPC: function(pc) {
    let winID = pc._winID;
    if (this._list[winID]) {
      this._list[winID].push(Components.utils.getWeakReference(pc));
    } else {
      this._list[winID] = [Components.utils.getWeakReference(pc)];
    }
    this.removeNullRefs(winID);
  },

  removeNullRefs: function(winID) {
    if (this._list === undefined || this._list[winID] === undefined) {
      return;
    }
    this._list[winID] = this._list[winID].filter(
      function (e,i,a) { return e.get() !== null; });
  },

  hasActivePeerConnection: function(winID) {
    this.removeNullRefs(winID);
    return this._list[winID] ? true : false;
  },

  observe: function(subject, topic, data) {
    if (topic == "inner-window-destroyed") {
      let winID = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
      if (this._list[winID]) {
        this._list[winID].forEach(function(pcref) {
          let pc = pcref.get();
          if (pc !== null) {
            pc._pc.close(false);
            delete pc._observer;
            pc._pc = null;
          }
        });
        delete this._list[winID];
      }
    } else if (topic == "profile-change-net-teardown" ||
               topic == "network:offline-about-to-go-offline") {
      // Delete all peerconnections on shutdown - synchronously (we need
      // them to be done deleting transports before we return)!
      // Also kill them if "Work Offline" is selected - more can be created
      // while offline, but attempts to connect them should fail.
      let array;
      while ((array = this._list.pop()) != undefined) {
        array.forEach(function(pcref) {
          let pc = pcref.get();
          if (pc !== null) {
            pc._pc.close(true);
            delete pc._observer;
            pc._pc = null;
          }
        });
      };
      this._networkdown = true;
    }
    else if (topic == "network:offline-status-changed") {
      if (data == "offline") {
	// this._list shold be empty here
        this._networkdown = true;
      } else if (data == "online") {
        this._networkdown = false;
      }
    }
  },
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

  constructor: function(win, candidateInitDict) {
    if (this._win) {
      throw new Components.Exception("Constructor already called");
    }
    this._win = win;
    if (candidateInitDict !== undefined) {
      this.candidate = candidateInitDict.candidate || null;
      this.sdpMid = candidateInitDict.sdpMid || null;
      this.sdpMLineIndex = candidateInitDict.sdpMLineIndex === null ?
            null : candidateInitDict.sdpMLineIndex + 1;
    } else {
      this.candidate = this.sdpMid = this.sdpMLineIndex = null;
    }
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

  constructor: function(win, descriptionInitDict) {
    if (this._win) {
      throw new Components.Exception("Constructor already called");
    }
    this._win = win;
    if (descriptionInitDict !== undefined) {
      this.type = descriptionInitDict.type || null;
      this.sdp = descriptionInitDict.sdp || null;
    } else {
      this.type = this.sdp = null;
    }
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
  this._closed = false;

  this._onCreateOfferSuccess = null;
  this._onCreateOfferFailure = null;
  this._onCreateAnswerSuccess = null;
  this._onCreateAnswerFailure = null;

  this._pendingType = null;
  this._localType = null;
  this._remoteType = null;

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
  this.onopen = null;
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

  classInfo: Cu.getDOMClassInfo("RTCPeerConnection"),

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIDOMRTCPeerConnection,
    Ci.nsIDOMGlobalObjectConstructor,
    Ci.nsISupportsWeakReference
  ]),

  // Constructor is an explicit function, because of nsIDOMGlobalObjectConstructor.
  constructor: function(win, rtcConfig) {
    if (!Services.prefs.getBoolPref("media.peerconnection.enabled")) {
      throw new Components.Exception("PeerConnection not enabled (did you set the pref?)");
    }
    if (this._win) {
      throw new Components.Exception("RTCPeerConnection constructor already called");
    }
    if (!rtcConfig ||
        !Services.prefs.getBoolPref("media.peerconnection.use_document_iceservers")) {
      rtcConfig = {iceServers:
        JSON.parse(Services.prefs.getCharPref("media.peerconnection.default_iceservers"))};
    }
    this._mustValidateRTCConfiguration(rtcConfig,
        "RTCPeerConnection constructor passed invalid RTCConfiguration");
    if (_globalPCList._networkdown) {
      throw new Components.Exception("Can't create RTCPeerConnections when the network is down");
    }

    this._pc = Cc["@mozilla.org/peerconnection;1"].
             createInstance(Ci.IPeerConnection);
    this._observer = new PeerConnectionObserver(this);

    // Nothing starts until ICE gathering completes.
    this._queueOrRun({
      func: this._getPC().initialize,
      args: [this._observer, win, rtcConfig, Services.tm.currentThread],
      wait: true
    });

    this._win = win;
    this._winID = this._win.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;

    // Add a reference to the PeerConnection to global list.
    _globalPCList.addPC(this);
  },

  _getPC: function() {
    if (!this._pc) {
      throw new Components.Exception("PeerConnection is gone (did you turn on Offline mode?)");
    }
    return this._pc;
  },

  /**
   * Add a function to the queue or run it immediately if the queue is empty.
   * Argument is an object with the func, args and wait properties; wait should
   * be set to true if the function has a success/error callback that will
   * call _executeNext, false if it doesn't have a callback.
   */
  _queueOrRun: function(obj) {
    this._checkClosed();
    if (!this._pending) {
      if (obj.type !== undefined) {
        this._pendingType = obj.type;
      }
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
      if (obj.type !== undefined) {
        this._pendingType = obj.type;
      }
      obj.func.apply(this, obj.args);
      if (!obj.wait) {
        this._executeNext();
      }
    } else {
      this._pending = false;
    }
  },

  /**
   * An RTCConfiguration looks like this:
   *
   * { "iceServers": [ { url:"stun:23.21.150.121" },
   *                   { url:"turn:user@turn.example.org", credential:"mypass"} ] }
   *
   * We check for basic structure and well-formed stun/turn urls, but not
   * validity of servers themselves, before passing along to C++.
   * ErrorMsg is passed in to detail which array-entry failed, if any.
   */
  _mustValidateRTCConfiguration: function(rtcConfig, errorMsg) {
    function isObject(obj) {
      return obj && (typeof obj === "object");
    }
    function isArraylike(obj) {
      return isObject(obj) && ("length" in obj);
    }
    function nicerNewURI(uriStr, errorMsg) {
      let ios = Cc['@mozilla.org/network/io-service;1'].getService(Ci.nsIIOService);
      try {
        return ios.newURI(uriStr, null, null);
      } catch (e if (e.result == Cr.NS_ERROR_MALFORMED_URI)) {
        throw new Components.Exception(errorMsg + " - malformed URI: " + uriStr,
                                       Cr.NS_ERROR_MALFORMED_URI);
      }
    }
    function mustValidateServer(server) {
      let url = nicerNewURI(server.url, errorMsg);
      if (!(url.scheme in { stun:1, stuns:1, turn:1, turns:1 })) {
        throw new Components.Exception(errorMsg + " - improper scheme: " + url.scheme,
                                       Cr.NS_ERROR_MALFORMED_URI);
      }
      if (server.credential && isObject(server.credential)) {
        throw new Components.Exception(errorMsg + " - invalid credential");
      }
    }
    if (!isObject(rtcConfig)) {
      throw new Components.Exception(errorMsg);
    }
    if (!isArraylike(rtcConfig.iceServers)) {
      throw new Components.Exception(errorMsg +
                                     " - iceServers [] property not present");
    }
    let len = rtcConfig.iceServers.length;
    for (let i=0; i < len; i++) {
      mustValidateServer (rtcConfig.iceServers[i], errorMsg);
    }
  },

  /**
   * Constraints look like this:
   *
   * {
   *   mandatory: {"OfferToReceiveAudio": true, "OfferToReceiveVideo": true },
   *   optional: [{"VoiceActivityDetection": true}, {"FooBar": 10}]
   * }
   *
   * We check for basic structure of constraints and the validity of
   * mandatory constraints against those we support (fail if we don't).
   * Unknown optional constraints may be of any type.
   */
  _mustValidateConstraints: function(constraints, errorMsg) {
    function isObject(obj) {
      return obj && (typeof obj === "object");
    }
    function isArraylike(obj) {
      return isObject(obj) && ("length" in obj);
    }
    const SUPPORTED_CONSTRAINTS = {
      OfferToReceiveAudio:1,
      OfferToReceiveVideo:1,
      MozDontOfferDataChannel:1
    };
    const OTHER_KNOWN_CONSTRAINTS = {
      VoiceActivityDetection:1,
      IceTransports:1,
      RequestIdentity:1
    };
    // Parse-aid: Testing for pilot error of missing outer block avoids
    // otherwise silent no-op since both mandatory and optional are optional
    if (!isObject(constraints) || Array.isArray(constraints)) {
      throw new Components.Exception(errorMsg);
    }
    if (constraints.mandatory) {
      // Testing for pilot error of using [] on mandatory here throws nicer msg
      // (arrays would throw in loop below regardless but with more cryptic msg)
      if (!isObject(constraints.mandatory) || Array.isArray(constraints.mandatory)) {
        throw new Components.Exception(errorMsg + " - malformed mandatory constraints");
      }
      for (let constraint in constraints.mandatory) {
        if (!(constraint in SUPPORTED_CONSTRAINTS) &&
            constraints.mandatory.hasOwnProperty(constraint)) {
          throw new Components.Exception(errorMsg + " - " +
                                         ((constraint in OTHER_KNOWN_CONSTRAINTS)?
                                          "unsupported" : "unknown") +
                                         " mandatory constraint: " + constraint);
        }
      }
    }
    if (constraints.optional) {
      if (!isArraylike(constraints.optional)) {
        throw new Components.Exception(errorMsg +
                                       " - malformed optional constraint array");
      }
      let len = constraints.optional.length;
      for (let i = 0; i < len; i += 1) {
        if (!isObject(constraints.optional[i])) {
          throw new Components.Exception(errorMsg +
                                         " - malformed optional constraint: " +
                                         constraints.optional[i]);
        }
        let constraints_per_entry = 0;
        for (let constraint in constraints.optional[i]) {
          if (constraints.optional[i].hasOwnProperty(constraint)) {
            if (constraints_per_entry) {
              throw new Components.Exception(errorMsg +
                  " - optional constraint must be single key/value pair");
            }
            constraints_per_entry += 1;
          }
        }
      }
    }
  },

  // Ideally, this should be of the form _checkState(state),
  // where the state is taken from an enumeration containing
  // the valid peer connection states defined in the WebRTC
  // spec. See Bug 831756.
  _checkClosed: function() {
    if (this._closed) {
      throw new Components.Exception("Peer connection is closed");
    }
  },

  createOffer: function(onSuccess, onError, constraints) {
    if (!constraints) {
      constraints = {};
    }

    this._mustValidateConstraints(constraints, "createOffer passed invalid constraints");
    this._onCreateOfferSuccess = onSuccess;
    this._onCreateOfferFailure = onError;

    this._queueOrRun({
      func: this._getPC().createOffer,
      args: [constraints],
      wait: true
    });
  },

  _createAnswer: function(onSuccess, onError, constraints, provisional) {
    this._onCreateAnswerSuccess = onSuccess;
    this._onCreateAnswerFailure = onError;

    if (!this.remoteDescription) {

      this._observer.onCreateAnswerError(Ci.IPeerConnection.kInvalidState,
                                         "setRemoteDescription not called");
      return;
    }

    if (this.remoteDescription.type != "offer") {

      this._observer.onCreateAnswerError(Ci.IPeerConnection.kInvalidState,
                                         "No outstanding offer");
      return;
    }

    // TODO: Implement provisional answer.

    this._getPC().createAnswer(constraints);
  },

  createAnswer: function(onSuccess, onError, constraints, provisional) {
    if (!constraints) {
      constraints = {};
    }

    this._mustValidateConstraints(constraints, "createAnswer passed invalid constraints");

    if (!provisional) {
      provisional = false;
    }

    this._queueOrRun({
      func: this._createAnswer,
      args: [onSuccess, onError, constraints, provisional],
      wait: true
    });
  },

  setLocalDescription: function(desc, onSuccess, onError) {
    // TODO -- if we have two setLocalDescriptions in the
    // queue,this code overwrites the callbacks for the first
    // one with the callbacks for the second one. See Bug 831759.
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
        throw new Components.Exception("Invalid type " + desc.type +
                                       " provided to setLocalDescription");
        break;
    }

    this._queueOrRun({
      func: this._getPC().setLocalDescription,
      args: [type, desc.sdp],
      wait: true,
      type: desc.type
    });
  },

  setRemoteDescription: function(desc, onSuccess, onError) {
    // TODO -- if we have two setRemoteDescriptions in the
    // queue, this code overwrites the callbacks for the first
    // one with the callbacks for the second one. See Bug 831759.
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
        throw new Components.Exception("Invalid type " + desc.type +
                                       " provided to setRemoteDescription");
        break;
    }

    this._queueOrRun({
      func: this._getPC().setRemoteDescription,
      args: [type, desc.sdp],
      wait: true,
      type: desc.type
    });
  },

  updateIce: function(config, constraints, restart) {
    return Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  addIceCandidate: function(cand, onSuccess, onError) {
    if (!cand) {
      throw new Components.Exception("NULL candidate passed to addIceCandidate!");
    }

    if (!cand.candidate || !cand.sdpMLineIndex) {
      throw new Components.Exception("Invalid candidate passed to addIceCandidate!");
    }

    this._onAddIceCandidateSuccess = onSuccess;
    this._onAddIceCandidateError = onError;

    this._queueOrRun({
      func: this._getPC().addIceCandidate,
      args: [cand.candidate, cand.sdpMid || "", cand.sdpMLineIndex],
      wait: true
    });
  },

  addStream: function(stream, constraints) {
    // TODO: Implement constraints.
    this._queueOrRun({
      func: this._getPC().addStream,
      args: [stream],
      wait: false
    });
  },

  removeStream: function(stream) {
     //Bug844295: Not implemeting this functionality.
     return Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  close: function() {
    this._queueOrRun({
      func: this._getPC().close,
      args: [false],
      wait: false
    });
    this._closed = true;
  },

  get localStreams() {
    this._checkClosed();
    return this._getPC().localStreams;
  },

  get remoteStreams() {
    this._checkClosed();
    return this._getPC().remoteStreams;
  },

  get localDescription() {
    this._checkClosed();
    let sdp = this._getPC().localDescription;
    if (sdp.length == 0) {
      return null;
    }
    return {
      type: this._localType, sdp: sdp,
      __exposedProps__: { type: "rw", sdp: "rw" }
    };
  },

  get remoteDescription() {
    this._checkClosed();
    let sdp = this._getPC().remoteDescription;
    if (sdp.length == 0) {
      return null;
    }
    return {
      type: this._remoteType, sdp: sdp,
      __exposedProps__: { type: "rw", sdp: "rw" }
    };
  },

  get readyState() {
    // checking for our local pc closed indication
    // before invoking the pc methods.
    if(this._closed) {
      return "closed";
    }

    var state="undefined";
    switch (this._getPC().readyState) {
      case Ci.IPeerConnection.kNew:
        state = "new";
        break;
      case Ci.IPeerConnection.kNegotiating:
        state = "negotiating";
        break;
      case Ci.IPeerConnection.kActive:
        state = "active";
        break;
      case Ci.IPeerConnection.kClosing:
        state = "closing";
        break;
      case Ci.IPeerConnection.kClosed:
        state = "closed";
        break;
    }
    return state;
  },

  createDataChannel: function(label, dict) {
    this._checkClosed();
    if (dict == undefined) {
	dict = {};
    }
    if (dict.maxRetransmitTime != undefined &&
        dict.maxRetransmitNum != undefined) {
      throw new Components.Exception("Both maxRetransmitTime and maxRetransmitNum cannot be provided");
    }
    let protocol;
    if (dict.protocol == undefined) {
      protocol = "";
    } else {
      protocol = dict.protocol;
    }

    // Must determine the type where we still know if entries are undefined.
    let type;
    if (dict.maxRetransmitTime != undefined) {
      type = Ci.IPeerConnection.kDataChannelPartialReliableTimed;
    } else if (dict.maxRetransmitNum != undefined) {
      type = Ci.IPeerConnection.kDataChannelPartialReliableRexmit;
    } else {
      type = Ci.IPeerConnection.kDataChannelReliable;
    }

    // Synchronous since it doesn't block.
    let channel = this._getPC().createDataChannel(
      label, protocol, type, dict.outOfOrderAllowed, dict.maxRetransmitTime,
      dict.maxRetransmitNum, dict.preset ? true : false,
      dict.stream != undefined ? dict.stream : 0xFFFF
    );
    return channel;
  },

  connectDataConnection: function(localport, remoteport, numstreams) {
    if (numstreams == undefined || numstreams <= 0) {
      numstreams = 16;
    }
    this._queueOrRun({
      func: this._getPC().connectDataConnection,
      args: [localport, remoteport, numstreams],
      wait: false
    });
  }
};

function RTCError(code, message) {
  this.name = this.reasonName[Math.min(code, this.reasonName.length - 1)];
  this.message = (typeof message === "string")? message : this.name;
  this.__exposedProps__ = { name: "rw", message: "rw" };
}
RTCError.prototype = {
  // These strings must match those defined in the WebRTC spec.
  reasonName: [
    "NO_ERROR", // Should never happen -- only used for testing
    "INVALID_CONSTRAINTS_TYPE",
    "INVALID_CANDIDATE_TYPE",
    "INVALID_MEDIASTREAM_TRACK",
    "INVALID_STATE",
    "INVALID_SESSION_DESCRIPTION",
    "INCOMPATIBLE_SESSION_DESCRIPTION",
    "INCOMPATIBLE_CONSTRAINTS",
    "INCOMPATIBLE_MEDIASTREAMTRACK",
    "INTERNAL_ERROR"
  ]
};

// This is a separate object because we don't want to expose it to DOM.
function PeerConnectionObserver(dompc) {
  this._dompc = dompc;
}
PeerConnectionObserver.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.IPeerConnectionObserver,
                                         Ci.nsISupportsWeakReference]),

  callCB: function(callback, arg) {
    if (callback) {
      try {
        callback.onCallback(arg);
      } catch(e) {
        // A content script (user-provided) callback threw an error. We don't
        // want this to take down peerconnection, but we still want the user
        // to see it, so we catch it, report it, and move on.
        //
        // We do stack parsing in two different places for different reasons:

        var msg;
        if (e.result == Cr.NS_ERROR_XPC_JS_THREW_JS_OBJECT) {
          // TODO(jib@mozilla.com): Revisit once bug 862153 is fixed.
          //
          // The actual content script frame is unavailable due to bug 862153,
          // so users see file and line # into this file, which is not helpful.
          //
          // 1) Fix up the error message itself to differentiate between the
          //    22 places we call callCB() in this file, using plain JS stack.
          //
          // Tweak the existing NS_ERROR_XPC_JS_THREW_JS_OBJECT message:
          // -'Error: x' when calling method: [RTCPeerConCallback::onCallback]
          // +'Error: x' when calling method: [RTCPeerConCallback::onCreateOfferError]

          let caller = Error().stack.split("\n")[1].split("@")[0];
          // caller ~= "PeerConnectionObserver.prototype.onCreateOfferError"

          msg = e.message.replace("::onCallback", "::" + caller.split(".")[2]);
        } else {
          msg = e.message;
        }

        // Log error message to web console and window.onerror, if present.
        //
        // 2) nsIScriptError doesn't understand the nsIStackFrame format, so
        //    do the translation by extracting file and line from XPCOM stack:
        //
        // e.location ~= "JS frame :: file://.js :: RTCPCCb::onCallback :: line 1"

        let stack = e.location.toString().split(" :: ");
        let file = stack[1];
        let line = parseInt(stack[3].split(" ")[1]);

        let scriptErrorClass = Cc["@mozilla.org/scripterror;1"];
        let scriptError = scriptErrorClass.createInstance(Ci.nsIScriptError);
        scriptError.initWithWindowID(msg, file, null, line, 0,
                                     Ci.nsIScriptError.exceptionFlag,
                                     "content javascript",
                                     this._dompc._winID);
        let console = Cc["@mozilla.org/consoleservice;1"].
            getService(Ci.nsIConsoleService);
        console.logMessage(scriptError);

        // Safely call onerror directly if present (necessary for testing)
        try {
          if (typeof this._dompc._win.onerror === "function") {
            this._dompc._win.onerror(msg, file, line);
          }
        } catch(e) {
          // If onerror itself throws, service it.
          try {
            let scriptError = scriptErrorClass.createInstance(Ci.nsIScriptError);
            scriptError.initWithWindowID(e.message, e.fileName, null,
                                         e.lineNumber, 0,
                                         Ci.nsIScriptError.exceptionFlag,
                                         "content javascript",
                                         this._dompc._winID);
            console.logMessage(scriptError);
          } catch(e) {}
        }
      }
    }
  },

  onCreateOfferSuccess: function(offer) {
    this.callCB(this._dompc._onCreateOfferSuccess,
                { type: "offer", sdp: offer,
                __exposedProps__: { type: "rw", sdp: "rw" } });
    this._dompc._executeNext();
  },

  onCreateOfferError: function(code, message) {
    this.callCB(this._dompc._onCreateOfferFailure, new RTCError(code, message));
    this._dompc._executeNext();
  },

  onCreateAnswerSuccess: function(answer) {
    this.callCB (this._dompc._onCreateAnswerSuccess,
                 { type: "answer", sdp: answer,
                 __exposedProps__: { type: "rw", sdp: "rw" } });
    this._dompc._executeNext();
  },

  onCreateAnswerError: function(code, message) {
    this.callCB(this._dompc._onCreateAnswerFailure, new RTCError(code, message));
    this._dompc._executeNext();
  },

  onSetLocalDescriptionSuccess: function() {
    this._dompc._localType = this._dompc._pendingType;
    this._dompc._pendingType = null;
    this.callCB(this._dompc._onSetLocalDescriptionSuccess);

    // Until we support generating trickle ICE candidates,
    // we go ahead and trigger a call of onicecandidate here.
    // This is to provide some level of compatibility with
    // scripts that expect this behavior (which is how Chrome
    // signals that no further trickle candidates will be sent).
    // TODO: This needs to be removed when Bug 842459 lands.
    this.foundIceCandidate(null);

    this._dompc._executeNext();
  },

  onSetRemoteDescriptionSuccess: function() {
    this._dompc._remoteType = this._dompc._pendingType;
    this._dompc._pendingType = null;
    this.callCB(this._dompc._onSetRemoteDescriptionSuccess);
    this._dompc._executeNext();
  },

  onSetLocalDescriptionError: function(code, message) {
    this._dompc._pendingType = null;
    this.callCB(this._dompc._onSetLocalDescriptionFailure,
                new RTCError(code, message));
    this._dompc._executeNext();
  },

  onSetRemoteDescriptionError: function(code, message) {
    this._dompc._pendingType = null;
    this.callCB(this._dompc._onSetRemoteDescriptionFailure,
                new RTCError(code, message));
    this._dompc._executeNext();
  },

  onAddIceCandidateSuccess: function() {
    this._dompc._pendingType = null;
    this.callCB(this._dompc._onAddIceCandidateSuccess);
    this._dompc._executeNext();
  },

  onAddIceCandidateError: function(code, message) {
    this._dompc._pendingType = null;
    this.callCB(this._dompc._onAddIceCandidateError, new RTCError(code, message));
    this._dompc._executeNext();
  },

  onStateChange: function(state) {
    if (state != Ci.IPeerConnectionObserver.kIceState) {
      return;
    }

    switch (this._dompc._pc.iceState) {
      case Ci.IPeerConnection.kIceGathering:
        this.callCB(this._dompc.ongatheringchange, "gathering");
        break;
      case Ci.IPeerConnection.kIceWaiting:
        this.callCB(this._dompc.onicechange, "starting");
        this._dompc._executeNext();
        break;
      case Ci.IPeerConnection.kIceChecking:
        this.callCB(this._dompc.onicechange, "checking");
        break;
      case Ci.IPeerConnection.kIceConnected:
        // ICE gathering complete.
        this.callCB(this._dompc.onicechange, "connected");
        this.callCB(this._dompc.ongatheringchange, "complete");
        break;
      case Ci.IPeerConnection.kIceFailed:
        this.callCB(this._dompc.onicechange, "failed");
        break;
      default:
        // Unknown state!
        break;
    }
  },

  onAddStream: function(stream, type) {
    this.callCB(this._dompc.onaddstream,
                { stream: stream, type: type,
                __exposedProps__: { stream: "r", type: "r" } });
  },

  onRemoveStream: function(stream, type) {
    this.callCB(this._dompc.onremovestream,
                { stream: stream, type: type,
                __exposedProps__: { stream: "r", type: "r" } });
  },

  foundIceCandidate: function(cand) {
    this.callCB(this._dompc.onicecandidate,
                {candidate: cand, __exposedProps__: { candidate: "rw" } });
  },

  notifyDataChannel: function(channel) {
    this.callCB(this._dompc.ondatachannel,
                { channel: channel, __exposedProps__: { channel: "r" } });
  },

  notifyConnection: function() {
    this.callCB (this._dompc.onconnection);
  },

  notifyClosedConnection: function() {
    this.callCB (this._dompc.onclosedconnection);
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory(
  [GlobalPCList, IceCandidate, SessionDescription, PeerConnection]
);
