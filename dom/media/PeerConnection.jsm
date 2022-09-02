/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const lazy = {};
ChromeUtils.defineModuleGetter(
  lazy,
  "PeerConnectionIdp",
  "resource://gre/modules/media/PeerConnectionIdp.jsm"
);

const PC_CONTRACT = "@mozilla.org/dom/peerconnection;1";
const PC_OBS_CONTRACT = "@mozilla.org/dom/peerconnectionobserver;1";
const PC_ICE_CONTRACT = "@mozilla.org/dom/rtcicecandidate;1";
const PC_SESSION_CONTRACT = "@mozilla.org/dom/rtcsessiondescription;1";
const PC_STATIC_CONTRACT = "@mozilla.org/dom/peerconnectionstatic;1";
const PC_COREQUEST_CONTRACT = "@mozilla.org/dom/createofferrequest;1";

const PC_CID = Components.ID("{bdc2e533-b308-4708-ac8e-a8bfade6d851}");
const PC_OBS_CID = Components.ID("{d1748d4c-7f6a-4dc5-add6-d55b7678537e}");
const PC_ICE_CID = Components.ID("{02b9970c-433d-4cc2-923d-f7028ac66073}");
const PC_SESSION_CID = Components.ID("{1775081b-b62d-4954-8ffe-a067bbf508a7}");
const PC_MANAGER_CID = Components.ID("{7293e901-2be3-4c02-b4bd-cbef6fc24f78}");
const PC_STATIC_CID = Components.ID("{0fb47c47-a205-4583-a9fc-cbadf8c95880}");
const PC_COREQUEST_CID = Components.ID(
  "{74b2122d-65a8-4824-aa9e-3d664cb75dc2}"
);

function logMsg(msg, file, line, flag, winID) {
  let scriptErrorClass = Cc["@mozilla.org/scripterror;1"];
  let scriptError = scriptErrorClass.createInstance(Ci.nsIScriptError);
  scriptError.initWithWindowID(
    `WebRTC: ${msg}`,
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
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),
  classID: PC_MANAGER_CID,
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
  QueryInterface: ChromeUtils.generateQI(["nsIDOMGlobalPropertyInitializer"]),
});

class RTCSessionDescription {
  init(win) {
    this._win = win;
    this._winID = this._win.windowGlobalChild.innerWindowId;
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
  QueryInterface: ChromeUtils.generateQI(["nsIDOMGlobalPropertyInitializer"]),
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
    this._pc = null;
    this._closed = false;

    // http://rtcweb-wg.github.io/jsep/#rfc.section.4.1.9
    // canTrickle == null means unknown; when a remote description is received it
    // is set to true or false based on the presence of the "trickle" ice-option
    this._canTrickle = null;

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

  // Pref-based overrides; will _not_ be reflected in getConfiguration
  _applyPrefsToConfig(rtcConfig) {
    if (
      rtcConfig.iceTransportPolicy == "all" &&
      Services.prefs.getBoolPref("media.peerconnection.ice.relay_only")
    ) {
      rtcConfig.iceTransportPolicy = "relay";
    }

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
        this._validateIceServers(
          rtcConfig.iceServers,
          "Ignoring invalid media.peerconnection.default_iceservers in about:config"
        );
      } catch (e) {
        this.logWarning(e.message);
        rtcConfig.iceServers = [];
      }
    }
  }

  _validateConfig(rtcConfig) {
    if ("sdpSemantics" in rtcConfig) {
      if (rtcConfig.sdpSemantics == "plan-b") {
        this.logWarning(
          `Outdated and non-standard {sdpSemantics: "plan-b"} is not ` +
            `supported! WebRTC may be unreliable. Please update code to ` +
            `follow standard "unified-plan".`
        );
      }
      // Don't let it show up in getConfiguration.
      delete rtcConfig.sdpSemantics;
    }

    if (this._config) {
      // certificates must match
      if (rtcConfig.certificates.length != this._config.certificates.length) {
        throw new this._win.DOMException(
          "Cannot change certificates with setConfiguration (length differs)",
          "InvalidModificationError"
        );
      }
      for (let i = 0; i < rtcConfig.certificates.length; i++) {
        if (rtcConfig.certificates[i] != this._config.certificates[i]) {
          throw new this._win.DOMException(
            `Cannot change certificates with setConfiguration ` +
              `(cert at index ${i} differs)`,
            "InvalidModificationError"
          );
        }
      }

      // bundlePolicy must match
      if (rtcConfig.bundlePolicy != this._config.bundlePolicy) {
        throw new this._win.DOMException(
          "Cannot change bundlePolicy with setConfiguration",
          "InvalidModificationError"
        );
      }

      // peerIdentity must match
      if (
        rtcConfig.peerIdentity &&
        rtcConfig.peerIdentity != this._config.peerIdentity
      ) {
        throw new this._win.DOMException(
          "Cannot change peerIdentity with setConfiguration",
          "InvalidModificationError"
        );
      }

      // TODO (bug 1339203): rtcpMuxPolicy must match
      // TODO (bug 1529398): iceCandidatePoolSize must match if sLD has ever
      // been called.
    }

    // This gets executed in the typical case when iceServers
    // are passed in through the web page.
    this._validateIceServers(
      rtcConfig.iceServers,
      "RTCPeerConnection constructor passed invalid RTCConfiguration"
    );
  }

  _checkIfIceRestartRequired(rtcConfig) {
    if (this._config) {
      if (rtcConfig.iceTransportPolicy != this._config.iceTransportPolicy) {
        this._pc.restartIceNoRenegotiationNeeded();
        return;
      }
      if (
        JSON.stringify(this._config.iceServers) !=
        JSON.stringify(rtcConfig.iceServers)
      ) {
        this._pc.restartIceNoRenegotiationNeeded();
      }
    }
  }

  __init(rtcConfig) {
    this._winID = this._win.windowGlobalChild.innerWindowId;
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

    this.__DOM_IMPL__._innerObject = this;
    const observer = new this._win.PeerConnectionObserver(this.__DOM_IMPL__);

    // Add a reference to the PeerConnection to global list (before init).
    _globalPCList.addPC(this);

    this._pc.initialize(observer, this._win);

    this.setConfiguration(rtcConfig);

    this._certificateReady = this._initCertificate(certificate);
    this._initIdp();
    _globalPCList.notifyLifecycleObservers(this, "initialized");
  }

  getConfiguration() {
    const config = Object.assign({}, this._config);
    delete config.sdpSemantics;
    return config;
  }

  setConfiguration(rtcConfig) {
    this._checkClosed();
    this._validateConfig(rtcConfig);
    this._checkIfIceRestartRequired(rtcConfig);

    // Allow prefs to tweak these settings before passing to c++, but hide all
    // of that from JS.
    const configWithPrefTweaks = Object.assign({}, rtcConfig);
    this._applyPrefsToConfig(configWithPrefTweaks);
    this._pc.setConfiguration(configWithPrefTweaks);

    this._config = Object.assign({}, rtcConfig);
  }

  async _initCertificate(certificate) {
    if (!certificate) {
      certificate = await this._win.RTCPeerConnection.generateCertificate({
        name: "ECDSA",
        namedCurve: "P-256",
      });
    }
    this._pc.certificate = certificate;
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
    this._localIdp = new lazy.PeerConnectionIdp(this._win, idpTimeout);
    this._remoteIdp = new lazy.PeerConnectionIdp(this._win, idpTimeout);
  }

  // Add a function to the internal operations chain.

  _chain(operation) {
    return this._pc.chain(operation);
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
  _validateIceServers(iceServers, msg) {
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
      if (!urls.length) {
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
        this.setEH(name, h);
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
        this.setEH(name, h);
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
    // This entry-point handles both new and legacy call sig. Decipher which one
    if (onSuccess) {
      return this._legacy(onSuccess, onErr, () => this._createOffer(options));
    }
    return this._async(() => this._createOffer(options));
  }

  // Ensures that we have at least one transceiver of |kind| that is
  // configured to receive. It will create one if necessary.
  _ensureOfferToReceive(kind) {
    let hasRecv = this.getTransceivers().some(
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

    this.getTransceivers()
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
      this._pc.createOffer(options);
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
      this._pc.createAnswer();
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
          let outerId = this._win.docShell.outerWindowID;

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

      try {
        await new Promise((resolve, reject) => {
          this._onSetDescriptionSuccess = resolve;
          this._onSetDescriptionFailure = reject;
          this._pc.setLocalDescription(this._actions[type], sdp);
        });
        await p;
      } catch (e) {
        this._pc.onSetDescriptionError();
        throw e;
      }
      await this._pc.onSetDescriptionSuccess(type, false);
    });
  }

  async _validateIdentity(sdp, origin) {
    // Only run a single identity verification at a time.  We have to do this to
    // avoid problems with the fact that identity validation doesn't block the
    // resolution of setRemoteDescription().
    const validate = async () => {
      // Access this._pc synchronously in case pc is closed later
      const identity = this._pc.peerIdentity;
      await this._lastIdentityValidation;
      const msg = await this._remoteIdp.verifyIdentityFromSDP(sdp, origin);
      // If this pc has an identity already, then the identity in sdp must match
      if (identity && (!msg || msg.identity !== identity)) {
        throw new this._win.DOMException(
          "Peer Identity mismatch, expected: " + identity,
          "OperationError"
        );
      }
      if (this._closed) {
        return;
      }
      if (msg) {
        // Set new identity and generate an event.
        this._pc.peerIdentity = msg.identity;
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
      if (this._closed) {
        return;
      }
      this._rejectPeerIdentity(e);

      // If we don't expect a specific peer identity, failure to get a valid
      // peer identity is not a terminal state, so replace the promise to
      // allow another attempt.
      if (!this._pc.peerIdentity) {
        this._resetPeerIdentityPromise();
      }
    });

    if (this._closed) {
      return;
    }
    // Only wait for IdP validation if we need identity matching
    if (this._pc.peerIdentity) {
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
      try {
        if (type == "offer" && this.signalingState == "have-local-offer") {
          await new Promise((resolve, reject) => {
            this._onSetDescriptionSuccess = resolve;
            this._onSetDescriptionFailure = reject;
            this._pc.setLocalDescription(
              Ci.IPeerConnection.kActionRollback,
              ""
            );
          });
          await this._pc.onSetDescriptionSuccess("rollback", false);
          this._updateCanTrickle();
        }

        if (this._closed) {
          return;
        }

        this._sanityCheckSdp(sdp);

        const p = this._getPermission();

        const haveSetRemote = new Promise((resolve, reject) => {
          this._onSetDescriptionSuccess = resolve;
          this._onSetDescriptionFailure = reject;
          this._pc.setRemoteDescription(this._actions[type], sdp);
        });

        if (type != "rollback") {
          // Do setRemoteDescription and identity validation in parallel
          await this._validateIdentity(sdp);
        }
        await p;
        await haveSetRemote;
      } catch (e) {
        this._pc.onSetDescriptionError();
        throw e;
      }

      await this._pc.onSetDescriptionSuccess(type, true);
      this._updateCanTrickle();
    });
  }

  setIdentityProvider(provider, { protocol, usernameHint, peerIdentity } = {}) {
    this._checkClosed();
    peerIdentity = peerIdentity || this._pc.peerIdentity;
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
      this._pc.fingerprint,
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
        !this._pc.pendingRemoteDescription.length &&
        !this._pc.currentRemoteDescription.length
      ) {
        throw new this._win.DOMException(
          "No remoteDescription.",
          "InvalidStateError"
        );
      }
      return new Promise((resolve, reject) => {
        this._onAddIceCandidateSuccess = resolve;
        this._onAddIceCandidateError = reject;
        this._pc.addIceCandidate(
          candidate,
          sdpMid || "",
          usernameFragment || "",
          sdpMLineIndex
        );
      });
    });
  }

  restartIce() {
    this._pc.restartIce();
  }

  addStream(stream) {
    stream.getTracks().forEach(track => this.addTrack(track, stream));
  }

  addTrack(track, ...streams) {
    this._checkClosed();

    if (
      this.getTransceivers().some(
        transceiver => transceiver.sender.track == track
      )
    ) {
      throw new this._win.DOMException(
        "This track is already set on a sender.",
        "InvalidAccessError"
      );
    }

    let transceiver = this.getTransceivers().find(transceiver => {
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
    this.updateNegotiationNeeded();
    return transceiver.sender;
  }

  removeTrack(sender) {
    this._checkClosed();

    if (!this._pc.createdSender(sender)) {
      throw new this._win.DOMException(
        "This sender was not created by this PeerConnection",
        "InvalidAccessError"
      );
    }

    let transceiver = this.getTransceivers().find(
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

    return this._pc.addTransceiver(init, kind, sendTrack);
  }

  addTransceiver(sendTrackOrKind, init) {
    this._checkClosed();
    let transceiver = this._addTransceiverNoEvents(sendTrackOrKind, init);
    this.updateNegotiationNeeded();
    return transceiver;
  }

  updateNegotiationNeeded() {
    this._pc.updateNegotiationNeeded();
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
    this._pc.close();
    this._suppressEvents = true;
  }

  getLocalStreams() {
    this._checkClosed();
    let localStreams = new Set();
    this.getTransceivers().forEach(transceiver => {
      transceiver.sender.getStreams().forEach(stream => {
        localStreams.add(stream);
      });
    });
    return [...localStreams.values()];
  }

  getRemoteStreams() {
    this._checkClosed();
    return this._pc.getRemoteStreams();
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
    this._pc.enablePacketDump(level, type, sending);
  }

  mozDisablePacketDump(level, type, sending) {
    this._pc.disablePacketDump(level, type, sending);
  }

  getTransceivers() {
    return this._pc.getTransceivers();
  }

  get localDescription() {
    return this.pendingLocalDescription || this.currentLocalDescription;
  }

  get currentLocalDescription() {
    this._checkClosed();
    const sdp = this._pc.currentLocalDescription;
    if (!sdp.length) {
      return null;
    }
    const type = this._pc.currentOfferer ? "offer" : "answer";
    return new this._win.RTCSessionDescription({ type, sdp });
  }

  get pendingLocalDescription() {
    this._checkClosed();
    const sdp = this._pc.pendingLocalDescription;
    if (!sdp.length) {
      return null;
    }
    const type = this._pc.pendingOfferer ? "offer" : "answer";
    return new this._win.RTCSessionDescription({ type, sdp });
  }

  get remoteDescription() {
    return this.pendingRemoteDescription || this.currentRemoteDescription;
  }

  get currentRemoteDescription() {
    this._checkClosed();
    const sdp = this._pc.currentRemoteDescription;
    if (!sdp.length) {
      return null;
    }
    const type = this._pc.currentOfferer ? "answer" : "offer";
    return new this._win.RTCSessionDescription({ type, sdp });
  }

  get pendingRemoteDescription() {
    this._checkClosed();
    const sdp = this._pc.pendingRemoteDescription;
    if (!sdp.length) {
      return null;
    }
    const type = this._pc.pendingOfferer ? "answer" : "offer";
    return new this._win.RTCSessionDescription({ type, sdp });
  }

  get peerIdentity() {
    return this._peerIdentity;
  }
  get idpLoginUrl() {
    return this._localIdp.idpLoginUrl;
  }
  get id() {
    return this._pc.id;
  }
  set id(s) {
    this._pc.id = s;
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
    return this._pc.signalingState;
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

    return this._auto(onSucc, onErr, () => this._pc.getStats(selector));
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
      dataChannel = this._pc.createDataChannel(
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
  QueryInterface: ChromeUtils.generateQI(["nsIDOMGlobalPropertyInitializer"]),
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

  fireNegotiationNeededEvent() {
    this.dispatchEvent(new this._win.Event("negotiationneeded"));
  }

  onPacket(level, type, sending, packet) {
    var pc = this._dompc;
    if (pc._onPacket) {
      pc._onPacket(level, type, sending, packet);
    }
  }
}
setupPrototype(PeerConnectionObserver, {
  classID: PC_OBS_CID,
  contractID: PC_OBS_CONTRACT,
  QueryInterface: ChromeUtils.generateQI(["nsIDOMGlobalPropertyInitializer"]),
});

class RTCPeerConnectionStatic {
  init(win) {
    this._winID = win.windowGlobalChild.innerWindowId;
  }

  registerPeerConnectionLifecycleCallback(cb) {
    _globalPCList._registerPeerConnectionLifecycleCallback(this._winID, cb);
  }
}
setupPrototype(RTCPeerConnectionStatic, {
  classID: PC_STATIC_CID,
  contractID: PC_STATIC_CONTRACT,
  QueryInterface: ChromeUtils.generateQI(["nsIDOMGlobalPropertyInitializer"]),
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
  "PeerConnectionObserver",
  "CreateOfferRequest",
];
