/* jshint moz:true, browser:true */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "PeerConnectionIdp",
  "resource://gre/modules/media/PeerConnectionIdp.jsm"
);

const PC_CONTRACT = "@mozilla.org/dom/peerconnection;1";
const PC_OBS_CONTRACT = "@mozilla.org/dom/peerconnectionobserver;1";
const PC_ICE_CONTRACT = "@mozilla.org/dom/rtcicecandidate;1";
const PC_SESSION_CONTRACT = "@mozilla.org/dom/rtcsessiondescription;1";
const PC_STATIC_CONTRACT = "@mozilla.org/dom/peerconnectionstatic;1";
const PC_SENDER_CONTRACT = "@mozilla.org/dom/rtpsender;1";
const PC_TRANSCEIVER_CONTRACT = "@mozilla.org/dom/rtptransceiver;1";
const PC_COREQUEST_CONTRACT = "@mozilla.org/dom/createofferrequest;1";

const PC_CID = Components.ID("{bdc2e533-b308-4708-ac8e-a8bfade6d851}");
const PC_OBS_CID = Components.ID("{d1748d4c-7f6a-4dc5-add6-d55b7678537e}");
const PC_ICE_CID = Components.ID("{02b9970c-433d-4cc2-923d-f7028ac66073}");
const PC_SESSION_CID = Components.ID("{1775081b-b62d-4954-8ffe-a067bbf508a7}");
const PC_MANAGER_CID = Components.ID("{7293e901-2be3-4c02-b4bd-cbef6fc24f78}");
const PC_STATIC_CID = Components.ID("{0fb47c47-a205-4583-a9fc-cbadf8c95880}");
const PC_SENDER_CID = Components.ID("{4fff5d46-d827-4cd4-a970-8fd53977440e}");
const PC_TRANSCEIVER_CID = Components.ID(
  "{09475754-103a-41f5-a2d0-e1f27eb0b537}"
);
const PC_COREQUEST_CID = Components.ID(
  "{74b2122d-65a8-4824-aa9e-3d664cb75dc2}"
);

function logMsg(msg, file, line, flag, winID) {
  let scriptErrorClass = Cc["@mozilla.org/scripterror;1"];
  let scriptError = scriptErrorClass.createInstance(Ci.nsIScriptError);
  scriptError.initWithWindowID(
    msg,
    file,
    null,
    line,
    0,
    flag,
    "content javascript",
    winID
  );
  Services.console.logMessage(scriptError);
}

let setupPrototype = (_class, dict) => {
  _class.prototype.classDescription = _class.name;
  Object.assign(_class.prototype, dict);
};

// Global list of PeerConnection objects, so they can be cleaned up when
// a page is torn down. (Maps inner window ID to an array of PC objects).
class GlobalPCList {
  constructor() {
    this._list = {};
    this._networkdown = false; // XXX Need to query current state somehow
    this._lifecycleobservers = {};
    this._nextId = 1;
    Services.obs.addObserver(this, "inner-window-destroyed", true);
    Services.obs.addObserver(this, "profile-change-net-teardown", true);
    Services.obs.addObserver(this, "network:offline-about-to-go-offline", true);
    Services.obs.addObserver(this, "network:offline-status-changed", true);
    Services.obs.addObserver(this, "gmp-plugin-crash", true);
    Services.obs.addObserver(this, "PeerConnection:response:allow", true);
    Services.obs.addObserver(this, "PeerConnection:response:deny", true);
    if (Services.cpmm) {
      Services.cpmm.addMessageListener("gmp-plugin-crash", this);
    }
  }

  notifyLifecycleObservers(pc, type) {
    for (var key of Object.keys(this._lifecycleobservers)) {
      this._lifecycleobservers[key](pc, pc._winID, type);
    }
  }

  addPC(pc) {
    let winID = pc._winID;
    if (this._list[winID]) {
      this._list[winID].push(Cu.getWeakReference(pc));
    } else {
      this._list[winID] = [Cu.getWeakReference(pc)];
    }
    pc._globalPCListId = this._nextId++;
    this.removeNullRefs(winID);
  }

  findPC(globalPCListId) {
    for (let winId in this._list) {
      if (this._list.hasOwnProperty(winId)) {
        for (let pcref of this._list[winId]) {
          let pc = pcref.get();
          if (pc && pc._globalPCListId == globalPCListId) {
            return pc;
          }
        }
      }
    }
    return null;
  }

  removeNullRefs(winID) {
    if (this._list[winID] === undefined) {
      return;
    }
    this._list[winID] = this._list[winID].filter(function(e, i, a) {
      return e.get() !== null;
    });

    if (this._list[winID].length === 0) {
      delete this._list[winID];
    }
  }

  handleGMPCrash(data) {
    let broadcastPluginCrash = function(list, winID, pluginID, pluginName) {
      if (list.hasOwnProperty(winID)) {
        list[winID].forEach(function(pcref) {
          let pc = pcref.get();
          if (pc) {
            pc._pc.pluginCrash(pluginID, pluginName);
          }
        });
      }
    };

    // a plugin crashed; if it's associated with any of our PCs, fire an
    // event to the DOM window
    for (let winId in this._list) {
      broadcastPluginCrash(this._list, winId, data.pluginID, data.pluginName);
    }
  }

  receiveMessage({ name, data }) {
    if (name == "gmp-plugin-crash") {
      this.handleGMPCrash(data);
    }
  }

  observe(subject, topic, data) {
    let cleanupPcRef = function(pcref) {
      let pc = pcref.get();
      if (pc) {
        pc._suppressEvents = true;
        pc.close();
      }
    };

    let cleanupWinId = function(list, winID) {
      if (list.hasOwnProperty(winID)) {
        list[winID].forEach(cleanupPcRef);
        delete list[winID];
      }
    };

    if (topic == "inner-window-destroyed") {
      let winID = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
      cleanupWinId(this._list, winID);

      if (this._lifecycleobservers.hasOwnProperty(winID)) {
        delete this._lifecycleobservers[winID];
      }
    } else if (
      topic == "profile-change-net-teardown" ||
      topic == "network:offline-about-to-go-offline"
    ) {
      // As Necko doesn't prevent us from accessing the network we still need to
      // monitor the network offline/online state here. See bug 1326483
      this._networkdown = true;
    } else if (topic == "network:offline-status-changed") {
      if (data == "offline") {
        this._networkdown = true;
      } else if (data == "online") {
        this._networkdown = false;
      }
    } else if (topic == "gmp-plugin-crash") {
      if (subject instanceof Ci.nsIWritablePropertyBag2) {
        let pluginID = subject.getPropertyAsUint32("pluginID");
        let pluginName = subject.getPropertyAsAString("pluginName");
        let data = { pluginID, pluginName };
        this.handleGMPCrash(data);
      }
    } else if (
      topic == "PeerConnection:response:allow" ||
      topic == "PeerConnection:response:deny"
    ) {
      var pc = this.findPC(data);
      if (pc) {
        if (topic == "PeerConnection:response:allow") {
          pc._settlePermission.allow();
        } else {
          let err = new pc._win.DOMException(
            "The request is not allowed by " +
              "the user agent or the platform in the current context.",
            "NotAllowedError"
          );
          pc._settlePermission.deny(err);
        }
      }
    }
  }

  _registerPeerConnectionLifecycleCallback(winID, cb) {
    this._lifecycleobservers[winID] = cb;
  }
}
setupPrototype(GlobalPCList, {
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
  ]),
  classID: PC_MANAGER_CID,
  _xpcom_factory: {
    createInstance(outer, iid) {
      if (outer) {
        throw Cr.NS_ERROR_NO_AGGREGATION;
      }
      return _globalPCList.QueryInterface(iid);
    },
  },
});

var _globalPCList = new GlobalPCList();

class RTCIceCandidate {
  init(win) {
    this._win = win;
  }

  __init(dict) {
    if (dict.sdpMid == null && dict.sdpMLineIndex == null) {
      throw new this._win.TypeError(
        "Either sdpMid or sdpMLineIndex must be specified"
      );
    }
    Object.assign(this, dict);
  }
}
setupPrototype(RTCIceCandidate, {
  classID: PC_ICE_CID,
  contractID: PC_ICE_CONTRACT,
  QueryInterface: ChromeUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer]),
});

class RTCSessionDescription {
  init(win) {
    this._win = win;
    this._winID = this._win.windowUtils.currentInnerWindowID;
  }

  __init({ type, sdp }) {
    if (!type) {
      throw new this._win.TypeError(
        "Missing required 'type' member of RTCSessionDescriptionInit"
      );
    }
    Object.assign(this, { _type: type, _sdp: sdp });
  }

  get type() {
    return this._type;
  }
  set type(type) {
    this.warn();
    this._type = type;
  }

  get sdp() {
    return this._sdp;
  }
  set sdp(sdp) {
    this.warn();
    this._sdp = sdp;
  }

  warn() {
    if (!this._warned) {
      // Warn once per RTCSessionDescription about deprecated writable usage.
      this.logWarning(
        "RTCSessionDescription's members are readonly! " +
          "Writing to them is deprecated and will break soon!"
      );
      this._warned = true;
    }
  }

  logWarning(msg) {
    let err = this._win.Error();
    logMsg(
      msg,
      err.fileName,
      err.lineNumber,
      Ci.nsIScriptError.warningFlag,
      this._winID
    );
  }
}
setupPrototype(RTCSessionDescription, {
  classID: PC_SESSION_CID,
  contractID: PC_SESSION_CONTRACT,
  QueryInterface: ChromeUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer]),
});

// Records PC related telemetry
class PeerConnectionTelemetry {
  // ICE connection state enters connected or completed.
  recordConnected() {
    Services.telemetry.scalarAdd("webrtc.peerconnection.connected", 1);
    this.recordConnected = () => {};
  }
  // DataChannel is created
  _recordDataChannelCreated() {
    Services.telemetry.scalarAdd(
      "webrtc.peerconnection.datachannel_created",
      1
    );
    this._recordDataChannelCreated = () => {};
  }
  // DataChannel initialized with maxRetransmitTime
  _recordMaxRetransmitTime(maxRetransmitTime) {
    if (maxRetransmitTime === undefined) {
      return false;
    }
    Services.telemetry.scalarAdd(
      "webrtc.peerconnection.datachannel_max_retx_used",
      1
    );
    this._recordMaxRetransmitTime = () => true;
    return true;
  }
  // DataChannel initialized with maxPacketLifeTime
  _recordMaxPacketLifeTime(maxPacketLifeTime) {
    if (maxPacketLifeTime === undefined) {
      return false;
    }
    Services.telemetry.scalarAdd(
      "webrtc.peerconnection.datachannel_max_life_used",
      1
    );
    this._recordMaxPacketLifeTime = () => true;
    return true;
  }
  // DataChannel initialized
  recordDataChannelInit(maxRetransmitTime, maxPacketLifeTime) {
    const retxUsed = this._recordMaxRetransmitTime(maxRetransmitTime);
    if (this._recordMaxPacketLifeTime(maxPacketLifeTime) && retxUsed) {
      Services.telemetry.scalarAdd(
        "webrtc.peerconnection.datachannel_max_retx_and_life_used",
        1
      );
      this.recordDataChannelInit = () => {};
    }
    this._recordDataChannelCreated();
  }
}

class RTCPeerConnection {
  constructor() {
    this._transceivers = [];

    this._pc = null;
    this._closed = false;

    // http://rtcweb-wg.github.io/jsep/#rfc.section.4.1.9
    // canTrickle == null means unknown; when a remote description is received it
    // is set to true or false based on the presence of the "trickle" ice-option
    this._canTrickle = null;
    this._localUfragsToReplace = new Set();

    // So we can record telemetry on state transitions
    this._iceConnectionState = "new";

    this._hasStunServer = this._hasTurnServer = false;
    this._iceGatheredRelayCandidates = false;
    // Records telemetry
    this._pcTelemetry = new PeerConnectionTelemetry();
  }

  init(win) {
    this._win = win;
  }

  __init(rtcConfig) {
    this._winID = this._win.windowUtils.currentInnerWindowID;
    // TODO: Update this code once we support pc.setConfiguration, to track
    // setting from content independently from pref (Bug 1181768).
    if (
      rtcConfig.iceTransportPolicy == "all" &&
      Services.prefs.getBoolPref("media.peerconnection.ice.relay_only")
    ) {
      rtcConfig.iceTransportPolicy = "relay";
    }
    this._config = Object.assign({}, rtcConfig);

    if (
      !rtcConfig.iceServers ||
      !Services.prefs.getBoolPref(
        "media.peerconnection.use_document_iceservers"
      )
    ) {
      try {
        rtcConfig.iceServers = JSON.parse(
          Services.prefs.getCharPref(
            "media.peerconnection.default_iceservers"
          ) || "[]"
        );
      } catch (e) {
        this.logWarning(
          "Ignoring invalid media.peerconnection.default_iceservers in about:config"
        );
        rtcConfig.iceServers = [];
      }
      try {
        this._mustValidateRTCConfiguration(
          rtcConfig,
          "Ignoring invalid media.peerconnection.default_iceservers in about:config"
        );
      } catch (e) {
        this.logWarning(e.message);
        rtcConfig.iceServers = [];
      }
    } else {
      // This gets executed in the typical case when iceServers
      // are passed in through the web page.
      this._mustValidateRTCConfiguration(
        rtcConfig,
        "RTCPeerConnection constructor passed invalid RTCConfiguration"
      );
    }

    let certificates = rtcConfig.certificates || [];

    if (certificates.some(c => c.expires <= Date.now())) {
      throw new this._win.DOMException(
        "Unable to create RTCPeerConnection with an expired certificate",
        "InvalidAccessError"
      );
    }

    // TODO(bug 1531875): Check origin of certs

    // TODO(bug 1176518): Remove this code once we support multiple certs
    let certificate;
    if (certificates.length == 1) {
      certificate = certificates[0];
    } else if (certificates.length) {
      throw new this._win.DOMException(
        "RTCPeerConnection does not currently support multiple certificates",
        "NotSupportedError"
      );
    }

    this._documentPrincipal = Cu.getWebIDLCallerPrincipal();

    if (_globalPCList._networkdown) {
      throw new this._win.DOMException(
        "Can't create RTCPeerConnections when the network is down",
        "InvalidStateError"
      );
    }

    this.makeGetterSetterEH("ontrack");
    this.makeLegacyGetterSetterEH(
      "onaddstream",
      "Use peerConnection.ontrack instead."
    );
    this.makeLegacyGetterSetterEH(
      "onaddtrack",
      "Use peerConnection.ontrack instead."
    );
    this.makeGetterSetterEH("onicecandidate");
    this.makeGetterSetterEH("onnegotiationneeded");
    this.makeGetterSetterEH("onsignalingstatechange");
    this.makeGetterSetterEH("ondatachannel");
    this.makeGetterSetterEH("oniceconnectionstatechange");
    this.makeGetterSetterEH("onicegatheringstatechange");
    this.makeGetterSetterEH("onidentityresult");
    this.makeGetterSetterEH("onpeeridentity");
    this.makeGetterSetterEH("onidpassertionerror");
    this.makeGetterSetterEH("onidpvalidationerror");

    this._pc = new this._win.PeerConnectionImpl();
    this._operations = [];
    this._updateNegotiationNeededOnEmptyChain = false;

    this.__DOM_IMPL__._innerObject = this;
    const observer = new this._win.PeerConnectionObserver(this.__DOM_IMPL__);

    // Add a reference to the PeerConnection to global list (before init).
    _globalPCList.addPC(this);

    this._impl.initialize(
      observer,
      this._win,
      rtcConfig,
      Services.tm.currentThread
    );

    this._certificateReady = this._initCertificate(certificate);
    this._initIdp();
    _globalPCList.notifyLifecycleObservers(this, "initialized");
  }

  get _impl() {
    if (!this._pc) {
      throw new this._win.DOMException(
        "RTCPeerConnection is gone (did you enter Offline mode?)",
        "InvalidStateError"
      );
    }
    return this._pc;
  }

  getConfiguration() {
    return this._config;
  }

  async _initCertificate(certificate) {
    if (!certificate) {
      certificate = await this._win.RTCPeerConnection.generateCertificate({
        name: "ECDSA",
        namedCurve: "P-256",
      });
    }
    // Is the PC still around after the await?
    if (!this._closed) {
      this._impl.certificate = certificate;
    }
  }

  _resetPeerIdentityPromise() {
    this._peerIdentity = new this._win.Promise((resolve, reject) => {
      this._resolvePeerIdentity = resolve;
      this._rejectPeerIdentity = reject;
    });
  }

  _initIdp() {
    this._resetPeerIdentityPromise();
    this._lastIdentityValidation = this._win.Promise.resolve();

    let prefName = "media.peerconnection.identity.timeout";
    let idpTimeout = Services.prefs.getIntPref(prefName);
    this._localIdp = new PeerConnectionIdp(this._win, idpTimeout);
    this._remoteIdp = new PeerConnectionIdp(this._win, idpTimeout);
  }

  // Add a function to the internal operations chain.

  _chain(operation) {
    let resolveP, rejectP;
    const p = new Promise((r, e) => {
      resolveP = r;
      rejectP = e;
    });
    this._operations.push(() => {
      operation().then(resolveP, rejectP);
      const doNextOperation = () => {
        if (this._closed) {
          return;
        }
        this._operations.shift();
        if (this._operations.length) {
          this._operations[0]();
        } else if (this._updateNegotiationNeededOnEmptyChain) {
          this._updateNegotiationNeededOnEmptyChain = false;
          this.updateNegotiationNeeded();
        }
      };
      p.finally(doNextOperation);
    });
    if (this._operations.length == 1) {
      this._operations[0]();
    }
    return p;
  }

  // It's basically impossible to use async directly in JSImplemented code,
  // because the implicit promise must be wrapped to the right type for content.
  //
  // The _async wrapper takes care of this. The _legacy wrapper implements
  // legacy callbacks in a manner that produces correct line-numbers in errors,
  // provided that methods validate their inputs before putting themselves on
  // the pc's operations chain.
  //
  // These wrappers also serve as guards against settling promises past close().

  _async(func) {
    return this._win.Promise.resolve(this._closeWrapper(func));
  }

  _legacy(...args) {
    return this._win.Promise.resolve(this._legacyCloseWrapper(...args));
  }

  _auto(onSucc, onErr, func) {
    return typeof onSucc == "function"
      ? this._legacy(onSucc, onErr, func)
      : this._async(func);
  }

  async _closeWrapper(func) {
    let closed = this._closed;
    try {
      let result = await func();
      if (!closed && this._closed) {
        await new Promise(() => {});
      }
      return result;
    } catch (e) {
      if (!closed && this._closed) {
        await new Promise(() => {});
      }
      throw e;
    }
  }

  async _legacyCloseWrapper(onSucc, onErr, func) {
    let wrapCallback = cb => result => {
      try {
        cb && cb(result);
      } catch (e) {
        this.logErrorAndCallOnError(e);
      }
    };

    try {
      wrapCallback(onSucc)(await func());
    } catch (e) {
      wrapCallback(onErr)(e);
    }
  }

  // This implements the fairly common "Queue a task" logic
  async _queueTaskWithClosedCheck(func) {
    const pc = this;
    return new this._win.Promise((resolve, reject) => {
      Services.tm.dispatchToMainThread({
        run() {
          try {
            if (!pc._closed) {
              func();
              resolve();
            }
          } catch (e) {
            reject(e);
          }
        },
      });
    });
  }

  /**
   * An RTCConfiguration may look like this:
   *
   * { "iceServers": [ { urls: "stun:stun.example.org", },
   *                   { url: "stun:stun.example.org", }, // deprecated version
   *                   { urls: ["turn:turn1.x.org", "turn:turn2.x.org"],
   *                     username:"jib", credential:"mypass"} ] }
   *
   * This function normalizes the structure of the input for rtcConfig.iceServers for us,
   * so we test well-formed stun/turn urls before passing along to C++.
   *   msg - Error message to detail which array-entry failed, if any.
   */
  _mustValidateRTCConfiguration({ iceServers }, msg) {
    // Normalize iceServers input
    iceServers.forEach(server => {
      if (typeof server.urls === "string") {
        server.urls = [server.urls];
      } else if (!server.urls && server.url) {
        // TODO: Remove support for legacy iceServer.url eventually (Bug 1116766)
        server.urls = [server.url];
        this.logWarning("RTCIceServer.url is deprecated! Use urls instead.");
      }
    });

    let nicerNewURI = uriStr => {
      try {
        return Services.io.newURI(uriStr);
      } catch (e) {
        if (e.result == Cr.NS_ERROR_MALFORMED_URI) {
          throw new this._win.DOMException(
            `${msg} - malformed URI: ${uriStr}`,
            "SyntaxError"
          );
        }
        throw e;
      }
    };

    var stunServers = 0;

    iceServers.forEach(({ urls, username, credential, credentialType }) => {
      if (!urls) {
        // TODO: Remove once url is deprecated (Bug 1369563)
        throw new this._win.TypeError(
          "Missing required 'urls' member of RTCIceServer"
        );
      }
      if (urls.length == 0) {
        throw new this._win.DOMException(
          `${msg} - urls is empty`,
          "SyntaxError"
        );
      }
      urls
        .map(url => nicerNewURI(url))
        .forEach(({ scheme, spec }) => {
          if (scheme in { turn: 1, turns: 1 }) {
            if (username == undefined) {
              throw new this._win.DOMException(
                `${msg} - missing username: ${spec}`,
                "InvalidAccessError"
              );
            }
            if (username.length > 512) {
              throw new this._win.DOMException(
                `${msg} - username longer then 512 bytes: ${username}`,
                "InvalidAccessError"
              );
            }
            if (credential == undefined) {
              throw new this._win.DOMException(
                `${msg} - missing credential: ${spec}`,
                "InvalidAccessError"
              );
            }
            if (credentialType != "password") {
              this.logWarning(
                `RTCConfiguration TURN credentialType \"${credentialType}\"` +
                  " is not yet implemented. Treating as password." +
                  " https://bugzil.la/1247616"
              );
            }
            this._hasTurnServer = true;
            stunServers += 1;
          } else if (scheme in { stun: 1, stuns: 1 }) {
            this._hasStunServer = true;
            stunServers += 1;
          } else {
            throw new this._win.DOMException(
              `${msg} - improper scheme: ${scheme}`,
              "SyntaxError"
            );
          }
          if (scheme in { stuns: 1 }) {
            this.logWarning(scheme.toUpperCase() + " is not yet supported.");
          }
          if (stunServers >= 5) {
            this.logError(
              "Using five or more STUN/TURN servers causes problems"
            );
          } else if (stunServers > 2) {
            this.logWarning(
              "Using more than two STUN/TURN servers slows down discovery"
            );
          }
        });
    });
  }

  // Ideally, this should be of the form _checkState(state),
  // where the state is taken from an enumeration containing
  // the valid peer connection states defined in the WebRTC
  // spec. See Bug 831756.
  _checkClosed() {
    if (this._closed) {
      throw new this._win.DOMException(
        "Peer connection is closed",
        "InvalidStateError"
      );
    }
  }

  dispatchEvent(event) {
    // PC can close while events are firing if there is an async dispatch
    // in c++ land. But let through "closed" signaling and ice connection events.
    if (!this._suppressEvents) {
      this.__DOM_IMPL__.dispatchEvent(event);
    }
  }

  // Log error message to web console and window.onerror, if present.
  logErrorAndCallOnError(e) {
    this.logMsg(
      e.message,
      e.fileName,
      e.lineNumber,
      Ci.nsIScriptError.errorFlag
    );

    // Safely call onerror directly if present (necessary for testing)
    try {
      if (typeof this._win.onerror === "function") {
        this._win.onerror(e.message, e.fileName, e.lineNumber);
      }
    } catch (e) {
      // If onerror itself throws, service it.
      try {
        this.logMsg(
          e.message,
          e.fileName,
          e.lineNumber,
          Ci.nsIScriptError.errorFlag
        );
      } catch (e) {}
    }
  }

  logError(msg) {
    this.logStackMsg(msg, Ci.nsIScriptError.errorFlag);
  }

  logWarning(msg) {
    this.logStackMsg(msg, Ci.nsIScriptError.warningFlag);
  }

  logStackMsg(msg, flag) {
    let err = this._win.Error();
    this.logMsg(msg, err.fileName, err.lineNumber, flag);
  }

  logMsg(msg, file, line, flag) {
    return logMsg(msg, file, line, flag, this._winID);
  }

  getEH(type) {
    return this.__DOM_IMPL__.getEventHandler(type);
  }

  setEH(type, handler) {
    this.__DOM_IMPL__.setEventHandler(type, handler);
  }

  makeGetterSetterEH(name) {
    Object.defineProperty(this, name, {
      get() {
        return this.getEH(name);
      },
      set(h) {
        return this.setEH(name, h);
      },
    });
  }

  makeLegacyGetterSetterEH(name, msg) {
    Object.defineProperty(this, name, {
      get() {
        return this.getEH(name);
      },
      set(h) {
        this.logWarning(name + " is deprecated! " + msg);
        return this.setEH(name, h);
      },
    });
  }

  createOffer(optionsOrOnSucc, onErr, options) {
    let onSuccess = null;
    if (typeof optionsOrOnSucc == "function") {
      onSuccess = optionsOrOnSucc;
    } else {
      options = optionsOrOnSucc;
    }
    if (this._localUfragsToReplace.size > 0) {
      options.iceRestart = true;
    }
    // This entry-point handles both new and legacy call sig. Decipher which one
    if (onSuccess) {
      return this._legacy(onSuccess, onErr, () => this._createOffer(options));
    }
    return this._async(() => this._createOffer(options));
  }

  // Ensures that we have at least one transceiver of |kind| that is
  // configured to receive. It will create one if necessary.
  _ensureOfferToReceive(kind) {
    let hasRecv = this._transceivers.some(
      transceiver =>
        transceiver.getKind() == kind &&
        (transceiver.direction == "sendrecv" ||
          transceiver.direction == "recvonly") &&
        !transceiver.stopped
    );

    if (!hasRecv) {
      this._addTransceiverNoEvents(kind, { direction: "recvonly" });
    }
  }

  // Handles offerToReceiveAudio/Video
  _ensureTransceiversForOfferToReceive(options) {
    if (options.offerToReceiveAudio) {
      this._ensureOfferToReceive("audio");
    }

    if (options.offerToReceiveVideo) {
      this._ensureOfferToReceive("video");
    }

    this._transceivers
      .filter(transceiver => {
        return (
          (options.offerToReceiveVideo === false &&
            transceiver.receiver.track.kind == "video") ||
          (options.offerToReceiveAudio === false &&
            transceiver.receiver.track.kind == "audio")
        );
      })
      .forEach(transceiver => {
        if (transceiver.direction == "sendrecv") {
          transceiver.setDirectionInternal("sendonly");
        } else if (transceiver.direction == "recvonly") {
          transceiver.setDirectionInternal("inactive");
        }
      });
  }

  _createOffer(options) {
    this._checkClosed();
    this._ensureTransceiversForOfferToReceive(options);
    this._syncTransceivers();
    return this._chain(() => this._createAnOffer(options));
  }

  async _createAnOffer(options = {}) {
    switch (this.signalingState) {
      case "stable":
      case "have-local-offer":
        break;
      default:
        throw new this._win.DOMException(
          `Cannot create offer in ${this.signalingState}`,
          "InvalidStateError"
        );
    }
    let haveAssertion;
    if (this._localIdp.enabled) {
      haveAssertion = this._getIdentityAssertion();
    }
    await this._getPermission();
    await this._certificateReady;
    let sdp = await new Promise((resolve, reject) => {
      this._onCreateOfferSuccess = resolve;
      this._onCreateOfferFailure = reject;
      this._impl.createOffer(options);
    });
    if (haveAssertion) {
      await haveAssertion;
      sdp = this._localIdp.addIdentityAttribute(sdp);
    }
    return Cu.cloneInto({ type: "offer", sdp }, this._win);
  }

  createAnswer(optionsOrOnSucc, onErr) {
    // This entry-point handles both new and legacy call sig. Decipher which one
    if (typeof optionsOrOnSucc == "function") {
      return this._legacy(optionsOrOnSucc, onErr, () => this._createAnswer({}));
    }
    return this._async(() => this._createAnswer(optionsOrOnSucc));
  }

  _createAnswer(options) {
    this._checkClosed();
    this._syncTransceivers();
    return this._chain(() => this._createAnAnswer());
  }

  async _createAnAnswer() {
    if (this.signalingState != "have-remote-offer") {
      throw new this._win.DOMException(
        `Cannot create answer in ${this.signalingState}`,
        "InvalidStateError"
      );
    }
    let haveAssertion;
    if (this._localIdp.enabled) {
      haveAssertion = this._getIdentityAssertion();
    }
    await this._getPermission();
    await this._certificateReady;
    let sdp = await new Promise((resolve, reject) => {
      this._onCreateAnswerSuccess = resolve;
      this._onCreateAnswerFailure = reject;
      this._impl.createAnswer();
    });
    if (haveAssertion) {
      await haveAssertion;
      sdp = this._localIdp.addIdentityAttribute(sdp);
    }
    return Cu.cloneInto({ type: "answer", sdp }, this._win);
  }

  async _getPermission() {
    if (!this._havePermission) {
      const privileged =
        this._documentPrincipal.isSystemPrincipal ||
        Services.prefs.getBoolPref("media.navigator.permission.disabled");

      if (privileged) {
        this._havePermission = Promise.resolve();
      } else {
        this._havePermission = new Promise((resolve, reject) => {
          this._settlePermission = { allow: resolve, deny: reject };
          let outerId = this._win.windowUtils.outerWindowID;

          let chrome = new CreateOfferRequest(
            outerId,
            this._winID,
            this._globalPCListId,
            false
          );
          let request = this._win.CreateOfferRequest._create(this._win, chrome);
          Services.obs.notifyObservers(request, "PeerConnection:request");
        });
      }
    }
    return this._havePermission;
  }

  _sanityCheckSdp(sdp) {
    // The fippo butter finger filter AKA non-ASCII chars
    // Note: SDP allows non-ASCII character in the subject (who cares?)
    // eslint-disable-next-line no-control-regex
    let pos = sdp.search(/[^\u0000-\u007f]/);
    if (pos != -1) {
      throw new this._win.DOMException(
        "SDP contains non ASCII characters at position " + pos,
        "InvalidParameterError"
      );
    }
  }

  setLocalDescription(desc, onSucc, onErr) {
    return this._auto(onSucc, onErr, () => this._setLocalDescription(desc));
  }

  _setLocalDescription({ type, sdp }) {
    if (type == "pranswer") {
      throw new this._win.DOMException(
        "pranswer not yet implemented",
        "NotSupportedError"
      );
    }
    this._checkClosed();
    return this._chain(async () => {
      // Avoid Promise.all ahead of synchronous part of spec algorithm, since it
      // defers. NOTE: The spec says to return an already-rejected promise in
      // some cases, which is difficult to achieve in practice from JS (would
      // require avoiding await and then() entirely), but we want to come as
      // close as we reasonably can.
      const p = this._getPermission();
      if (!type) {
        switch (this.signalingState) {
          case "stable":
          case "have-local-offer":
          case "have-remote-pranswer":
            type = "offer";
            break;
          default:
            type = "answer";
            break;
        }
      }
      if (!sdp) {
        if (type == "offer") {
          await this._createAnOffer();
        } else if (type == "answer") {
          await this._createAnAnswer();
        }
      } else {
        this._sanityCheckSdp(sdp);
      }
      await new Promise((resolve, reject) => {
        this._onSetDescriptionSuccess = resolve;
        this._onSetDescriptionFailure = reject;
        this._impl.setLocalDescription(this._actions[type], sdp);
      });
      await p;
      this._negotiationNeeded = false;
      if (type == "answer") {
        if (this._localUfragsToReplace.size > 0) {
          const ufrags = new Set(this._getUfragsWithPwds(sdp));
          if (![...this._localUfragsToReplace].some(uf => ufrags.has(uf))) {
            this._localUfragsToReplace.clear();
          }
        }
      }
      this.updateNegotiationNeeded();
    });
  }

  async _validateIdentity(sdp, origin) {
    // Only run a single identity verification at a time.  We have to do this to
    // avoid problems with the fact that identity validation doesn't block the
    // resolution of setRemoteDescription().
    const validate = async () => {
      await this._lastIdentityValidation;
      const msg = await this._remoteIdp.verifyIdentityFromSDP(sdp, origin);
      // If this pc has an identity already, then the identity in sdp must match
      if (
        this._impl.peerIdentity &&
        (!msg || msg.identity !== this._impl.peerIdentity)
      ) {
        throw new this._win.DOMException(
          "Peer Identity mismatch, expected: " + this._impl.peerIdentity,
          "OperationError"
        );
      }

      if (msg) {
        // Set new identity and generate an event.
        this._impl.peerIdentity = msg.identity;
        this._resolvePeerIdentity(
          Cu.cloneInto(
            {
              idp: this._remoteIdp.provider,
              name: msg.identity,
            },
            this._win
          )
        );
      }
    };

    const haveValidation = validate();

    // Always eat errors on this chain
    this._lastIdentityValidation = haveValidation.catch(() => {});

    // If validation fails, we have some work to do. Fork it so it cannot
    // interfere with the validation chain itself, even if the catch function
    // throws.
    haveValidation.catch(e => {
      this._rejectPeerIdentity(e);

      // If we don't expect a specific peer identity, failure to get a valid
      // peer identity is not a terminal state, so replace the promise to
      // allow another attempt.
      if (!this._impl.peerIdentity) {
        this._resetPeerIdentityPromise();
      }
    });

    // Only wait for IdP validation if we need identity matching
    if (this._impl.peerIdentity) {
      await haveValidation;
    }
  }

  setRemoteDescription(desc, onSucc, onErr) {
    return this._auto(onSucc, onErr, () => this._setRemoteDescription(desc));
  }

  _setRemoteDescription({ type, sdp }) {
    if (!type) {
      throw new this._win.TypeError(
        "Missing required 'type' member of RTCSessionDescriptionInit"
      );
    }
    if (type == "pranswer") {
      throw new this._win.DOMException(
        "pranswer not yet implemented",
        "NotSupportedError"
      );
    }
    this._checkClosed();
    return this._chain(async () => {
      const haveSetRemote = (async () => {
        if (type == "offer" && this.signalingState == "have-local-offer") {
          await new Promise((resolve, reject) => {
            this._onSetDescriptionSuccess = resolve;
            this._onSetDescriptionFailure = reject;
            this._impl.setLocalDescription(
              Ci.IPeerConnection.kActionRollback,
              ""
            );
          });
          this._transceivers = this._transceivers.filter(t => !t.shouldRemove);
          this._updateCanTrickle();
        }
        this._sanityCheckSdp(sdp);
        const p = this._getPermission();
        await new Promise((resolve, reject) => {
          this._onSetDescriptionSuccess = resolve;
          this._onSetDescriptionFailure = reject;
          this._impl.setRemoteDescription(this._actions[type], sdp);
        });
        await p;
        this._transceivers = this._transceivers.filter(t => !t.shouldRemove);
        this._updateCanTrickle();
      })();

      if (type != "rollback") {
        // Do setRemoteDescription and identity validation in parallel
        await this._validateIdentity(sdp);
      }
      await haveSetRemote;
      this._negotiationNeeded = false;
      if (type == "answer") {
        if (this._localUfragsToReplace.size > 0) {
          const ufrags = new Set(
            this._getUfragsWithPwds(this._impl.currentLocalDescription)
          );
          if (![...this._localUfragsToReplace].some(uf => ufrags.has(uf))) {
            this._localUfragsToReplace.clear();
          }
        }
      }
      this.updateNegotiationNeeded();
    });
  }

  setIdentityProvider(provider, { protocol, usernameHint, peerIdentity } = {}) {
    this._checkClosed();
    peerIdentity = peerIdentity || this._impl.peerIdentity;
    this._localIdp.setIdentityProvider(
      provider,
      protocol,
      usernameHint,
      peerIdentity
    );
  }

  async _getIdentityAssertion() {
    await this._certificateReady;
    return this._localIdp.getIdentityAssertion(
      this._impl.fingerprint,
      this._documentPrincipal.origin
    );
  }

  getIdentityAssertion() {
    this._checkClosed();
    return this._win.Promise.resolve(
      this._chain(() => this._getIdentityAssertion())
    );
  }

  get canTrickleIceCandidates() {
    return this._canTrickle;
  }

  _updateCanTrickle() {
    let containsTrickle = section => {
      let lines = section.toLowerCase().split(/(?:\r\n?|\n)/);
      return lines.some(line => {
        let prefix = "a=ice-options:";
        if (line.substring(0, prefix.length) !== prefix) {
          return false;
        }
        let tokens = line.substring(prefix.length).split(" ");
        return tokens.some(x => x === "trickle");
      });
    };

    let desc = null;
    try {
      // The getter for remoteDescription can throw if the pc is closed.
      desc = this.remoteDescription;
    } catch (e) {}
    if (!desc) {
      this._canTrickle = null;
      return;
    }

    let sections = desc.sdp.split(/(?:\r\n?|\n)m=/);
    let topSection = sections.shift();
    this._canTrickle =
      containsTrickle(topSection) || sections.every(containsTrickle);
  }

  addIceCandidate(cand, onSucc, onErr) {
    if (
      cand.candidate != "" &&
      cand.sdpMid == null &&
      cand.sdpMLineIndex == null
    ) {
      throw new this._win.TypeError(
        "Cannot add a candidate without specifying either sdpMid or sdpMLineIndex"
      );
    }
    return this._auto(onSucc, onErr, () => this._addIceCandidate(cand));
  }

  async _addIceCandidate({
    candidate,
    sdpMid,
    sdpMLineIndex,
    usernameFragment,
  }) {
    this._checkClosed();
    return this._chain(async () => {
      if (
        !this._impl.pendingRemoteDescription.length &&
        !this._impl.currentRemoteDescription.length
      ) {
        throw new this._win.DOMException(
          "No remoteDescription.",
          "InvalidStateError"
        );
      }
      return new Promise((resolve, reject) => {
        this._onAddIceCandidateSuccess = resolve;
        this._onAddIceCandidateError = reject;
        this._impl.addIceCandidate(
          candidate,
          sdpMid || "",
          usernameFragment || "",
          sdpMLineIndex
        );
      });
    });
  }

  restartIce() {
    this._localUfragsToReplace = new Set([
      ...this._getUfragsWithPwds(this._impl.currentLocalDescription),
      ...this._getUfragsWithPwds(this._impl.pendingLocalDescription),
    ]);
    this.updateNegotiationNeeded();
  }

  _getUfragsWithPwds(sdp) {
    return (
      sdp
        .split("\r\nm=")
        .map(block => block.split("\r\n"))
        .map(lines => [
          lines.find(l => l.startsWith("a=ice-ufrag:")),
          lines.find(l => l.startsWith("a=ice-pwd:")),
        ])
        // Even though our own SDP doesn't currently do this: JSEP says properties
        // found in the session (array[0]) apply to all m-lines that don't specify
        // them, like default values.
        .map(([a, b], i, array) => [a || array[0][0], b || array[0][1]])
        .filter(([a, b]) => a && b)
        .map(array => array.join())
    );
  }

  addStream(stream) {
    stream.getTracks().forEach(track => this.addTrack(track, stream));
  }

  addTrack(track, ...streams) {
    this._checkClosed();

    if (
      this._transceivers.some(transceiver => transceiver.sender.track == track)
    ) {
      throw new this._win.DOMException(
        "This track is already set on a sender.",
        "InvalidAccessError"
      );
    }

    let transceiver = this._transceivers.find(transceiver => {
      return (
        transceiver.sender.track == null &&
        transceiver.getKind() == track.kind &&
        !transceiver.stopped &&
        !transceiver.hasBeenUsedToSend()
      );
    });

    if (transceiver) {
      transceiver.sender.setTrack(track);
      transceiver.sender.setStreams(streams);
      if (transceiver.direction == "recvonly") {
        transceiver.setDirectionInternal("sendrecv");
      } else if (transceiver.direction == "inactive") {
        transceiver.setDirectionInternal("sendonly");
      }
    } else {
      transceiver = this._addTransceiverNoEvents(track, {
        streams,
        direction: "sendrecv",
      });
    }

    transceiver.setAddTrackMagic();
    transceiver.sync();
    this.updateNegotiationNeeded();
    return transceiver.sender;
  }

  removeTrack(sender) {
    this._checkClosed();

    sender.checkWasCreatedByPc(this.__DOM_IMPL__);

    let transceiver = this._transceivers.find(
      transceiver => !transceiver.stopped && transceiver.sender == sender
    );

    // If the transceiver was removed due to rollback, let it slide.
    if (!transceiver || !sender.track) {
      return;
    }

    sender.setTrack(null);
    if (transceiver.direction == "sendrecv") {
      transceiver.setDirectionInternal("recvonly");
    } else if (transceiver.direction == "sendonly") {
      transceiver.setDirectionInternal("inactive");
    }

    transceiver.sync();
    this.updateNegotiationNeeded();
  }

  _addTransceiverNoEvents(sendTrackOrKind, init) {
    let sendTrack = null;
    let kind;
    if (typeof sendTrackOrKind == "string") {
      kind = sendTrackOrKind;
      switch (kind) {
        case "audio":
        case "video":
          break;
        default:
          throw new this._win.TypeError("Invalid media kind");
      }
    } else {
      sendTrack = sendTrackOrKind;
      kind = sendTrack.kind;
    }

    let transceiverImpl = this._impl.createTransceiverImpl(kind, sendTrack);
    let transceiver = this._win.RTCRtpTransceiver._create(
      this._win,
      new RTCRtpTransceiver(this, transceiverImpl, init, kind, sendTrack)
    );
    transceiver.sync();
    this._transceivers.push(transceiver);
    return transceiver;
  }

  _onTransceiverNeeded(kind, transceiverImpl) {
    let init = { direction: "recvonly" };
    let transceiver = this._win.RTCRtpTransceiver._create(
      this._win,
      new RTCRtpTransceiver(this, transceiverImpl, init, kind, null)
    );
    transceiver.sync();
    this._transceivers.push(transceiver);
  }

  addTransceiver(sendTrackOrKind, init) {
    this._checkClosed();
    let transceiver = this._addTransceiverNoEvents(sendTrackOrKind, init);
    this.updateNegotiationNeeded();
    return transceiver;
  }

  _syncTransceivers() {
    this._transceivers.forEach(transceiver => transceiver.sync());
  }

  updateNegotiationNeeded() {
    if (this._operations.length) {
      this._updateNegotiationNeededOnEmptyChain = true;
      return;
    }
    this._queueTaskWithClosedCheck(() => {
      if (this._operations.length) {
        this._updateNegotiationNeededOnEmptyChain = true;
        return;
      }
      if (this.signalingState != "stable") {
        return;
      }

      const negotiationNeeded =
        this._impl.checkNegotiationNeeded() ||
        this._localUfragsToReplace.size > 0;
      if (!negotiationNeeded) {
        this._negotiationNeeded = false;
        return;
      }

      if (this._negotiationNeeded) {
        return;
      }

      this._negotiationNeeded = true;
      this.dispatchEvent(new this._win.Event("negotiationneeded"));
    });
  }

  _replaceTrackNoRenegotiation(transceiverImpl, withTrack) {
    this._impl.replaceTrackNoRenegotiation(transceiverImpl, withTrack);
  }

  close() {
    if (this._closed) {
      return;
    }
    this._closed = true;
    this.changeIceConnectionState("closed");
    if (this._localIdp) {
      this._localIdp.close();
    }
    if (this._remoteIdp) {
      this._remoteIdp.close();
    }
    if (!this._suppressEvents) {
      this._transceivers.forEach(t => t.setStopped());
    }
    this._impl.close();
    this._suppressEvents = true;
    delete this._pc;
  }

  getLocalStreams() {
    this._checkClosed();
    let localStreams = new Set();
    this._transceivers.forEach(transceiver => {
      transceiver.sender.getStreams().forEach(stream => {
        localStreams.add(stream);
      });
    });
    return [...localStreams.values()];
  }

  getRemoteStreams() {
    this._checkClosed();
    return this._impl.getRemoteStreams();
  }

  getSenders() {
    return this.getTransceivers()
      .filter(transceiver => !transceiver.stopped)
      .map(transceiver => transceiver.sender);
  }

  getReceivers() {
    return this.getTransceivers()
      .filter(transceiver => !transceiver.stopped)
      .map(transceiver => transceiver.receiver);
  }

  mozSetPacketCallback(callback) {
    this._onPacket = callback;
  }

  mozEnablePacketDump(level, type, sending) {
    this._impl.enablePacketDump(level, type, sending);
  }

  mozDisablePacketDump(level, type, sending) {
    this._impl.disablePacketDump(level, type, sending);
  }

  getTransceivers() {
    return this._transceivers;
  }

  get localDescription() {
    return this.pendingLocalDescription || this.currentLocalDescription;
  }

  get currentLocalDescription() {
    this._checkClosed();
    const sdp = this._impl.currentLocalDescription;
    if (sdp.length == 0) {
      return null;
    }
    const type = this._impl.currentOfferer ? "offer" : "answer";
    return new this._win.RTCSessionDescription({ type, sdp });
  }

  get pendingLocalDescription() {
    this._checkClosed();
    const sdp = this._impl.pendingLocalDescription;
    if (sdp.length == 0) {
      return null;
    }
    const type = this._impl.pendingOfferer ? "offer" : "answer";
    return new this._win.RTCSessionDescription({ type, sdp });
  }

  get remoteDescription() {
    return this.pendingRemoteDescription || this.currentRemoteDescription;
  }

  get currentRemoteDescription() {
    this._checkClosed();
    const sdp = this._impl.currentRemoteDescription;
    if (sdp.length == 0) {
      return null;
    }
    const type = this._impl.currentOfferer ? "answer" : "offer";
    return new this._win.RTCSessionDescription({ type, sdp });
  }

  get pendingRemoteDescription() {
    this._checkClosed();
    const sdp = this._impl.pendingRemoteDescription;
    if (sdp.length == 0) {
      return null;
    }
    const type = this._impl.pendingOfferer ? "answer" : "offer";
    return new this._win.RTCSessionDescription({ type, sdp });
  }

  get peerIdentity() {
    return this._peerIdentity;
  }
  get idpLoginUrl() {
    return this._localIdp.idpLoginUrl;
  }
  get id() {
    return this._impl.id;
  }
  set id(s) {
    this._impl.id = s;
  }
  get iceGatheringState() {
    return this._pc.iceGatheringState;
  }
  get iceConnectionState() {
    return this._iceConnectionState;
  }

  get signalingState() {
    // checking for our local pc closed indication
    // before invoking the pc methods.
    if (this._closed) {
      return "closed";
    }
    return this._impl.signalingState;
  }

  handleIceGatheringStateChange() {
    _globalPCList.notifyLifecycleObservers(this, "icegatheringstatechange");
    this.dispatchEvent(new this._win.Event("icegatheringstatechange"));
    if (this.iceGatheringState === "complete") {
      this.dispatchEvent(
        new this._win.RTCPeerConnectionIceEvent("icecandidate", {
          candidate: null,
        })
      );
    }
  }

  changeIceConnectionState(state) {
    if (state != this._iceConnectionState) {
      this._iceConnectionState = state;
      _globalPCList.notifyLifecycleObservers(this, "iceconnectionstatechange");
      if (!this._closed) {
        this.dispatchEvent(new this._win.Event("iceconnectionstatechange"));
      }
    }
  }

  getStats(selector, onSucc, onErr) {
    if (selector !== null) {
      let matchingSenders = this.getSenders().filter(s => s.track === selector);
      let matchingReceivers = this.getReceivers().filter(
        r => r.track === selector
      );

      if (matchingSenders.length + matchingReceivers.length != 1) {
        throw new this._win.DOMException(
          "track must be associated with a unique sender or receiver, but " +
            " is associated with " +
            matchingSenders.length +
            " senders and " +
            matchingReceivers.length +
            " receivers.",
          "InvalidAccessError"
        );
      }
    }

    return this._auto(onSucc, onErr, () => this._impl.getStats(selector));
  }

  createDataChannel(
    label,
    {
      maxRetransmits,
      ordered,
      negotiated,
      id = null,
      maxRetransmitTime,
      maxPacketLifeTime,
      protocol,
    } = {}
  ) {
    this._checkClosed();
    this._pcTelemetry.recordDataChannelInit(
      maxRetransmitTime,
      maxPacketLifeTime
    );

    if (maxPacketLifeTime === undefined) {
      maxPacketLifeTime = maxRetransmitTime;
    }

    if (maxRetransmitTime !== undefined) {
      this.logWarning(
        "Use maxPacketLifeTime instead of deprecated maxRetransmitTime which will stop working soon in createDataChannel!"
      );
    }

    if (protocol.length > 32767) {
      // At least 65536/2 UTF-16 characters. UTF-8 might be too long.
      // Spec says to check how long |protocol| and |label| are in _bytes_. This
      // is a little ambiguous. For now, examine the length of the utf-8 encoding.
      const byteCounter = new TextEncoder("utf-8");

      if (byteCounter.encode(protocol).length > 65535) {
        throw new this._win.TypeError(
          "protocol cannot be longer than 65535 bytes"
        );
      }
    }

    if (label.length > 32767) {
      const byteCounter = new TextEncoder("utf-8");
      if (byteCounter.encode(label).length > 65535) {
        throw new this._win.TypeError(
          "label cannot be longer than 65535 bytes"
        );
      }
    }

    if (!negotiated) {
      id = null;
    } else if (id === null) {
      throw new this._win.TypeError("id is required when negotiated is true");
    }
    if (maxPacketLifeTime !== undefined && maxRetransmits !== undefined) {
      throw new this._win.TypeError(
        "Both maxPacketLifeTime and maxRetransmits cannot be provided"
      );
    }
    if (id == 65535) {
      throw new this._win.TypeError("id cannot be 65535");
    }
    // Must determine the type where we still know if entries are undefined.
    let type;
    if (maxPacketLifeTime !== undefined) {
      type = Ci.IPeerConnection.kDataChannelPartialReliableTimed;
    } else if (maxRetransmits !== undefined) {
      type = Ci.IPeerConnection.kDataChannelPartialReliableRexmit;
    } else {
      type = Ci.IPeerConnection.kDataChannelReliable;
    }
    // Synchronous since it doesn't block.
    let dataChannel;
    try {
      dataChannel = this._impl.createDataChannel(
        label,
        protocol,
        type,
        ordered,
        maxPacketLifeTime,
        maxRetransmits,
        negotiated,
        id
      );
    } catch (e) {
      if (e.result != Cr.NS_ERROR_NOT_AVAILABLE) {
        throw e;
      }

      const msg =
        id === null ? "No available id could be generated" : "Id is in use";
      throw new this._win.DOMException(msg, "OperationError");
    }

    // Spec says to only do this if this is the first DataChannel created,
    // but the c++ code that does the "is negotiation needed" checking will
    // only ever return true on the first one.
    this.updateNegotiationNeeded();

    return dataChannel;
  }
}
setupPrototype(RTCPeerConnection, {
  classID: PC_CID,
  contractID: PC_CONTRACT,
  QueryInterface: ChromeUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer]),
  _actions: {
    offer: Ci.IPeerConnection.kActionOffer,
    answer: Ci.IPeerConnection.kActionAnswer,
    pranswer: Ci.IPeerConnection.kActionPRAnswer,
    rollback: Ci.IPeerConnection.kActionRollback,
  },
});

// This is a separate class because we don't want to expose it to DOM.

class PeerConnectionObserver {
  init(win) {
    this._win = win;
  }

  __init(dompc) {
    this._dompc = dompc._innerObject;
  }

  newError({ message, name }) {
    return new this._dompc._win.DOMException(message, name);
  }

  dispatchEvent(event) {
    this._dompc.dispatchEvent(event);
  }

  onCreateOfferSuccess(sdp) {
    this._dompc._onCreateOfferSuccess(sdp);
  }

  onCreateOfferError(error) {
    this._dompc._onCreateOfferFailure(this.newError(error));
  }

  onCreateAnswerSuccess(sdp) {
    this._dompc._onCreateAnswerSuccess(sdp);
  }

  onCreateAnswerError(error) {
    this._dompc._onCreateAnswerFailure(this.newError(error));
  }

  onSetDescriptionSuccess() {
    this._dompc._onSetDescriptionSuccess();
  }

  onSetDescriptionError(error) {
    this._dompc._onSetDescriptionFailure(this.newError(error));
  }

  onAddIceCandidateSuccess() {
    this._dompc._onAddIceCandidateSuccess();
  }

  onAddIceCandidateError(error) {
    this._dompc._onAddIceCandidateError(this.newError(error));
  }

  onIceCandidate(sdpMLineIndex, sdpMid, candidate, usernameFragment) {
    let win = this._dompc._win;
    if (candidate || sdpMid || usernameFragment) {
      if (candidate.includes(" typ relay ")) {
        this._dompc._iceGatheredRelayCandidates = true;
      }
      candidate = new win.RTCIceCandidate({
        candidate,
        sdpMid,
        sdpMLineIndex,
        usernameFragment,
      });
    }
    this.dispatchEvent(
      new win.RTCPeerConnectionIceEvent("icecandidate", { candidate })
    );
  }

  // This method is primarily responsible for updating iceConnectionState.
  // This state is defined in the WebRTC specification as follows:
  //
  // iceConnectionState:
  // -------------------
  //   new           Any of the RTCIceTransports are in the new state and none
  //                 of them are in the checking, failed or disconnected state.
  //
  //   checking      Any of the RTCIceTransports are in the checking state and
  //                 none of them are in the failed or disconnected state.
  //
  //   connected     All RTCIceTransports are in the connected, completed or
  //                 closed state and at least one of them is in the connected
  //                 state.
  //
  //   completed     All RTCIceTransports are in the completed or closed state
  //                 and at least one of them is in the completed state.
  //
  //   failed        Any of the RTCIceTransports are in the failed state.
  //
  //   disconnected  Any of the RTCIceTransports are in the disconnected state
  //                 and none of them are in the failed state.
  //
  //   closed        All of the RTCIceTransports are in the closed state.

  handleIceConnectionStateChange(iceConnectionState) {
    let pc = this._dompc;
    if (pc.iceConnectionState === iceConnectionState) {
      return;
    }

    if (iceConnectionState === "failed") {
      if (!pc._hasStunServer) {
        pc.logError(
          "ICE failed, add a STUN server and see about:webrtc for more details"
        );
      } else if (!pc._hasTurnServer) {
        pc.logError(
          "ICE failed, add a TURN server and see about:webrtc for more details"
        );
      } else if (pc._hasTurnServer && !pc._iceGatheredRelayCandidates) {
        pc.logError(
          "ICE failed, your TURN server appears to be broken, see about:webrtc for more details"
        );
      } else {
        pc.logError("ICE failed, see about:webrtc for more details");
      }
    }

    pc.changeIceConnectionState(iceConnectionState);
  }

  onStateChange(state) {
    if (!this._dompc) {
      return;
    }

    if (state == "SignalingState") {
      this.dispatchEvent(new this._win.Event("signalingstatechange"));
      return;
    }

    if (!this._dompc._pc) {
      return;
    }

    switch (state) {
      case "IceConnectionState":
        let connState = this._dompc._pc.iceConnectionState;
        this._dompc._queueTaskWithClosedCheck(() => {
          this.handleIceConnectionStateChange(connState);
        });
        break;

      case "IceGatheringState":
        this._dompc.handleIceGatheringStateChange();
        break;

      default:
        this._dompc.logWarning("Unhandled state type: " + state);
        break;
    }
  }

  onTransceiverNeeded(kind, transceiverImpl) {
    this._dompc._onTransceiverNeeded(kind, transceiverImpl);
  }

  notifyDataChannel(channel) {
    this.dispatchEvent(
      new this._dompc._win.RTCDataChannelEvent("datachannel", { channel })
    );
  }

  fireTrackEvent(receiver, streams) {
    const pc = this._dompc;
    const transceiver = pc.getTransceivers().find(t => t.receiver == receiver);
    if (!transceiver) {
      return;
    }
    const track = receiver.track;
    this.dispatchEvent(
      new this._win.RTCTrackEvent("track", {
        transceiver,
        receiver,
        track,
        streams,
      })
    );
    // Fire legacy event as well for a little bit.
    this.dispatchEvent(
      new this._win.MediaStreamTrackEvent("addtrack", { track })
    );
  }

  fireStreamEvent(stream) {
    const ev = new this._win.MediaStreamEvent("addstream", { stream });
    this.dispatchEvent(ev);
  }

  onPacket(level, type, sending, packet) {
    var pc = this._dompc;
    if (pc._onPacket) {
      pc._onPacket(level, type, sending, packet);
    }
  }

  syncTransceivers() {
    this._dompc._syncTransceivers();
  }
}
setupPrototype(PeerConnectionObserver, {
  classID: PC_OBS_CID,
  contractID: PC_OBS_CONTRACT,
  QueryInterface: ChromeUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer]),
});

class RTCPeerConnectionStatic {
  init(win) {
    this._winID = win.windowUtils.currentInnerWindowID;
  }

  registerPeerConnectionLifecycleCallback(cb) {
    _globalPCList._registerPeerConnectionLifecycleCallback(this._winID, cb);
  }
}
setupPrototype(RTCPeerConnectionStatic, {
  classID: PC_STATIC_CID,
  contractID: PC_STATIC_CONTRACT,
  QueryInterface: ChromeUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer]),
});

class RTCRtpSender {
  constructor(pc, transceiverImpl, transceiver, track, kind, streams) {
    let dtmf = null;
    if (kind == "audio") {
      dtmf = transceiverImpl.dtmf;
    }

    Object.assign(this, {
      _pc: pc,
      _transceiverImpl: transceiverImpl,
      _transceiver: transceiver,
      track,
      _streams: streams,
      dtmf,
    });
  }

  replaceTrack(withTrack) {
    // async functions in here return a chrome promise, which is not something
    // content can use. This wraps that promise in something content can use.
    return this._pc._win.Promise.resolve(this._replaceTrack(withTrack));
  }

  async _replaceTrack(withTrack) {
    let pc = this._pc;
    if (withTrack && withTrack.kind != this._transceiver.getKind()) {
      throw new pc._win.TypeError("Cannot replaceTrack with a different kind!");
    }

    pc._checkClosed();

    await pc._chain(async () => {
      if (this._transceiver.stopped) {
        throw new pc._win.DOMException(
          "Cannot call replaceTrack when transceiver is stopped",
          "InvalidStateError"
        );
      }

      await pc._queueTaskWithClosedCheck(() => {
        // Updates the track on the MediaPipeline, will throw on failure.
        try {
          pc._replaceTrackNoRenegotiation(this._transceiverImpl, withTrack);
        } catch (e) {
          throw new pc._win.DOMException(
            "Track could not be replaced without renegotiation",
            "InvalidModificationError"
          );
        }
        this.track = withTrack;
        this._transceiver.sync();
      });
    });
  }

  setParameters(parameters) {
    return this._pc._win.Promise.resolve(this._setParameters(parameters));
  }

  async _setParameters(parameters) {
    this._pc._checkClosed();

    if (this._transceiver.stopped) {
      throw new this._pc._win.DOMException(
        "This sender's transceiver is stopped",
        "InvalidStateError"
      );
    }

    if (!Services.prefs.getBoolPref("media.peerconnection.simulcast")) {
      return;
    }

    parameters.encodings = parameters.encodings || [];

    parameters.encodings.reduce(
      (uniqueRids, { rid, scaleResolutionDownBy }) => {
        if (scaleResolutionDownBy < 1.0) {
          throw new this._pc._win.RangeError(
            "scaleResolutionDownBy must be >= 1.0"
          );
        }
        if (!rid && parameters.encodings.length > 1) {
          throw new this._pc._win.TypeError("Missing rid");
        }
        if (uniqueRids[rid]) {
          throw new this._pc._win.TypeError("Duplicate rid");
        }
        uniqueRids[rid] = true;
        return uniqueRids;
      },
      {}
    );

    // TODO(bug 1401592): transaction ids, timing changes

    await this._pc._queueTaskWithClosedCheck(() => {
      this.parameters = parameters;
      this._transceiver.sync();
    });
  }

  getParameters() {
    // TODO(bug 1401592): transaction ids

    // All the other stuff that the spec says to update is handled when
    // transceivers are synced.
    return this.parameters;
  }

  setStreams(streams) {
    this._streams = streams;
  }

  getStreams() {
    return this._streams;
  }

  setTrack(track) {
    this._pc._replaceTrackNoRenegotiation(this._transceiverImpl, track);
    this.track = track;
  }

  getStats() {
    if (this.track) {
      return this._pc._async(async () => this._pc._impl.getStats(this.track));
    }
    return this._pc._win.Promise.resolve().then(
      () => new this._pc._win.RTCStatsReport()
    );
  }

  checkWasCreatedByPc(pc) {
    if (pc != this._pc.__DOM_IMPL__) {
      throw new this._pc._win.DOMException(
        "This sender was not created by this PeerConnection",
        "InvalidAccessError"
      );
    }
  }
}
setupPrototype(RTCRtpSender, {
  classID: PC_SENDER_CID,
  contractID: PC_SENDER_CONTRACT,
  QueryInterface: ChromeUtils.generateQI([]),
});

class RTCRtpTransceiver {
  constructor(pc, transceiverImpl, init, kind, sendTrack) {
    let receiver = transceiverImpl.receiver;
    let streams = (init && init.streams) || [];
    let sender = pc._win.RTCRtpSender._create(
      pc._win,
      new RTCRtpSender(pc, transceiverImpl, this, sendTrack, kind, streams)
    );

    let direction = (init && init.direction) || "sendrecv";
    Object.assign(this, {
      _pc: pc,
      mid: null,
      sender,
      receiver,
      stopped: false,
      _direction: direction,
      currentDirection: null,
      addTrackMagic: false,
      shouldRemove: false,
      _hasBeenUsedToSend: false,
      // the receiver starts out without a track, so record this here
      _kind: kind,
      _transceiverImpl: transceiverImpl,
    });
  }

  set direction(direction) {
    this._pc._checkClosed();

    if (this.stopped) {
      throw new this._pc._win.DOMException(
        "Transceiver is stopped!",
        "InvalidStateError"
      );
    }

    if (this._direction == direction) {
      return;
    }

    this._direction = direction;
    this.sync();
    this._pc.updateNegotiationNeeded();
  }

  get direction() {
    return this._direction;
  }

  setDirectionInternal(direction) {
    this._direction = direction;
  }

  stop() {
    this._pc._checkClosed();

    if (this.stopped) {
      return;
    }

    this.setStopped();
    this.sync();
    this._pc.updateNegotiationNeeded();
  }

  setStopped() {
    this.stopped = true;
    this.currentDirection = null;
  }

  getKind() {
    return this._kind;
  }

  hasBeenUsedToSend() {
    return this._hasBeenUsedToSend;
  }

  setAddTrackMagic() {
    this.addTrackMagic = true;
  }

  sync() {
    if (this._syncing) {
      throw new DOMException("Reentrant sync! This is a bug!", "InternalError");
    }
    this._syncing = true;
    this._transceiverImpl.syncWithJS(this.__DOM_IMPL__);
    this._syncing = false;
  }

  // Used by _transceiverImpl.syncWithJS, don't call sync again!
  setCurrentDirection(direction) {
    if (this.stopped) {
      return;
    }

    switch (direction) {
      case "sendrecv":
      case "sendonly":
        this._hasBeenUsedToSend = true;
        break;
      default:
    }

    this.currentDirection = direction;
  }

  // Used by _transceiverImpl.syncWithJS, don't call sync again!
  setMid(mid) {
    this.mid = mid;
  }

  // Used by _transceiverImpl.syncWithJS, don't call sync again!
  unsetMid() {
    this.mid = null;
  }
}

setupPrototype(RTCRtpTransceiver, {
  classID: PC_TRANSCEIVER_CID,
  contractID: PC_TRANSCEIVER_CONTRACT,
  QueryInterface: ChromeUtils.generateQI([]),
});

class CreateOfferRequest {
  constructor(windowID, innerWindowID, callID, isSecure) {
    Object.assign(this, { windowID, innerWindowID, callID, isSecure });
  }
}
setupPrototype(CreateOfferRequest, {
  classID: PC_COREQUEST_CID,
  contractID: PC_COREQUEST_CONTRACT,
  QueryInterface: ChromeUtils.generateQI([]),
});

var EXPORTED_SYMBOLS = [
  "GlobalPCList",
  "RTCIceCandidate",
  "RTCSessionDescription",
  "RTCPeerConnection",
  "RTCPeerConnectionStatic",
  "RTCRtpSender",
  "RTCRtpTransceiver",
  "PeerConnectionObserver",
  "CreateOfferRequest",
];
