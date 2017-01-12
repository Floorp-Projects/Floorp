/* jshint moz:true, browser:true */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PeerConnectionIdp",
  "resource://gre/modules/media/PeerConnectionIdp.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "convertToRTCStatsReport",
  "resource://gre/modules/media/RTCStatsReport.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
  "resource://gre/modules/AppConstants.jsm");

const PC_CONTRACT = "@mozilla.org/dom/peerconnection;1";
const PC_OBS_CONTRACT = "@mozilla.org/dom/peerconnectionobserver;1";
const PC_ICE_CONTRACT = "@mozilla.org/dom/rtcicecandidate;1";
const PC_SESSION_CONTRACT = "@mozilla.org/dom/rtcsessiondescription;1";
const PC_MANAGER_CONTRACT = "@mozilla.org/dom/peerconnectionmanager;1";
const PC_STATS_CONTRACT = "@mozilla.org/dom/rtcstatsreport;1";
const PC_STATIC_CONTRACT = "@mozilla.org/dom/peerconnectionstatic;1";
const PC_SENDER_CONTRACT = "@mozilla.org/dom/rtpsender;1";
const PC_RECEIVER_CONTRACT = "@mozilla.org/dom/rtpreceiver;1";
const PC_COREQUEST_CONTRACT = "@mozilla.org/dom/createofferrequest;1";
const PC_DTMF_SENDER_CONTRACT = "@mozilla.org/dom/rtcdtmfsender;1";

const PC_CID = Components.ID("{bdc2e533-b308-4708-ac8e-a8bfade6d851}");
const PC_OBS_CID = Components.ID("{d1748d4c-7f6a-4dc5-add6-d55b7678537e}");
const PC_ICE_CID = Components.ID("{02b9970c-433d-4cc2-923d-f7028ac66073}");
const PC_SESSION_CID = Components.ID("{1775081b-b62d-4954-8ffe-a067bbf508a7}");
const PC_MANAGER_CID = Components.ID("{7293e901-2be3-4c02-b4bd-cbef6fc24f78}");
const PC_STATS_CID = Components.ID("{7fe6e18b-0da3-4056-bf3b-440ef3809e06}");
const PC_STATIC_CID = Components.ID("{0fb47c47-a205-4583-a9fc-cbadf8c95880}");
const PC_SENDER_CID = Components.ID("{4fff5d46-d827-4cd4-a970-8fd53977440e}");
const PC_RECEIVER_CID = Components.ID("{d974b814-8fde-411c-8c45-b86791b81030}");
const PC_COREQUEST_CID = Components.ID("{74b2122d-65a8-4824-aa9e-3d664cb75dc2}");
const PC_DTMF_SENDER_CID = Components.ID("{3610C242-654E-11E6-8EC0-6D1BE389A607}");

function logMsg(msg, file, line, flag, winID) {
  let scriptErrorClass = Cc["@mozilla.org/scripterror;1"];
  let scriptError = scriptErrorClass.createInstance(Ci.nsIScriptError);
  scriptError.initWithWindowID(msg, file, null, line, 0, flag,
                               "content javascript", winID);
  let console = Cc["@mozilla.org/consoleservice;1"].
  getService(Ci.nsIConsoleService);
  console.logMessage(scriptError);
};

let setupPrototype = (_class, dict) => {
  _class.prototype.classDescription = _class.name;
  Object.assign(_class.prototype, dict);
}

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
    if (Cc["@mozilla.org/childprocessmessagemanager;1"]) {
      let mm = Cc["@mozilla.org/childprocessmessagemanager;1"].getService(Ci.nsIMessageListenerManager);
      mm.addMessageListener("gmp-plugin-crash", this);
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
  }

  removeNullRefs(winID) {
    if (this._list[winID] === undefined) {
      return;
    }
    this._list[winID] = this._list[winID].filter(
      function (e,i,a) { return e.get() !== null; });

    if (this._list[winID].length === 0) {
      delete this._list[winID];
    }
  }

  hasActivePeerConnection(winID) {
    this.removeNullRefs(winID);
    return this._list[winID] ? true : false;
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
        pc._pc.close();
        delete pc._observer;
        pc._pc = null;
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
    } else if (topic == "profile-change-net-teardown" ||
               topic == "network:offline-about-to-go-offline") {
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
    } else if (topic == "PeerConnection:response:allow" ||
               topic == "PeerConnection:response:deny") {
      var pc = this.findPC(data);
      if (pc) {
        if (topic == "PeerConnection:response:allow") {
          pc._settlePermission.allow();
        } else {
          let err = new pc._win.DOMException("The request is not allowed by " +
              "the user agent or the platform in the current context.",
              "NotAllowedError");
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
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsIMessageListener,
                                         Ci.nsISupportsWeakReference,
                                         Ci.IPeerConnectionManager]),
  classID: PC_MANAGER_CID,
  _xpcom_factory: {
    createInstance: function(outer, iid) {
      if (outer) {
        throw Cr.NS_ERROR_NO_AGGREGATION;
      }
      return _globalPCList.QueryInterface(iid);
    }
  }
});

var _globalPCList = new GlobalPCList();

class RTCIceCandidate {
  init(win) {
    this._win = win;
  }

  __init(dict) {
    Object.assign(this, dict);
  }
}
setupPrototype(RTCIceCandidate, {
  classID: PC_ICE_CID,
  contractID: PC_ICE_CONTRACT,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer])
});

class RTCSessionDescription {
  init(win) {
    this._win = win;
    this._winID = this._win.QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;
  }

  __init({ type, sdp }) {
    Object.assign(this, { _type: type, _sdp: sdp });
  }

  get type() { return this._type; }
  set type(type) {
    this.warn();
    this._type = type;
  }

  get sdp() { return this._sdp; }
  set sdp(sdp) {
    this.warn();
    this._sdp = sdp;
  }

  warn() {
    if (!this._warned) {
      // Warn once per RTCSessionDescription about deprecated writable usage.
      this.logWarning("RTCSessionDescription's members are readonly! " +
                      "Writing to them is deprecated and will break soon!");
      this._warned = true;
    }
  }

  logWarning(msg) {
    let err = this._win.Error();
    logMsg(msg, err.fileName, err.lineNumber, Ci.nsIScriptError.warningFlag,
           this._winID);
  }
}
setupPrototype(RTCSessionDescription, {
  classID: PC_SESSION_CID,
  contractID: PC_SESSION_CONTRACT,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer])
});

class RTCStatsReport {
  constructor(win, dict) {
    this._win = win;
    this._pcid = dict.pcid;
    this._report = convertToRTCStatsReport(dict);
  }

  setInternal(aKey, aObj) {
    return this.__DOM_IMPL__.__set(aKey, aObj);
  }

  // TODO: Remove legacy API eventually
  // see Bug 1328194
  //
  // Since maplike is recent, we still also make the stats available as legacy
  // enumerable read-only properties directly on our content-facing object.
  // Must be called after our webidl sandwich is made.

  makeStatsPublic(warnNullable, isLegacy) {
    let legacyProps = {};
    for (let key in this._report) {
      let internal = Cu.cloneInto(this._report[key], this._win);
      if (isLegacy) {
        internal.type = this._specToLegacyFieldMapping[internal.type] || internal.type;
      }
      this.setInternal(key, internal);
      let value = Cu.cloneInto(this._report[key], this._win);
      value.type = this._specToLegacyFieldMapping[value.type] || value.type;
      legacyProps[key] = {
        enumerable: true, configurable: false,
        get: Cu.exportFunction(function() {
          if (warnNullable.warn) {
            warnNullable.warn();
            warnNullable.warn = null;
          }
          return value;
        }, this.__DOM_IMPL__.wrappedJSObject)
      };
    }
    Object.defineProperties(this.__DOM_IMPL__.wrappedJSObject, legacyProps);
  }

  get mozPcid() { return this._pcid; }
}
setupPrototype(RTCStatsReport, {
  classID: PC_STATS_CID,
  contractID: PC_STATS_CONTRACT,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports]),
  _specToLegacyFieldMapping: {
        'inbound-rtp' : 'inboundrtp',
        'outbound-rtp':'outboundrtp',
        'candidate-pair':'candidatepair',
        'local-candidate':'localcandidate',
        'remote-candidate':'remotecandidate'
  }
});

class RTCPeerConnection {
  constructor() {
    this._senders = [];
    this._receivers = [];

    this._pc = null;
    this._closed = false;

    this._localType = null;
    this._remoteType = null;
    // http://rtcweb-wg.github.io/jsep/#rfc.section.4.1.9
    // canTrickle == null means unknown; when a remote description is received it
    // is set to true or false based on the presence of the "trickle" ice-option
    this._canTrickle = null;

    // States
    this._iceGatheringState = this._iceConnectionState = "new";

    this._hasStunServer = this._hasTurnServer = false;
    this._iceGatheredRelayCandidates = false;

  // TODO: Remove legacy API eventually
  // see Bug 1328194
  this._onGetStatsIsLegacy = false;
  }

  init(win) {
    this._win = win;
  }

  __init(rtcConfig) {
    this._winID = this._win.QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;
    // TODO: Update this code once we support pc.setConfiguration, to track
    // setting from content independently from pref (Bug 1181768).
    if (rtcConfig.iceTransportPolicy == "all" &&
        Services.prefs.getBoolPref("media.peerconnection.ice.relay_only")) {
      rtcConfig.iceTransportPolicy = "relay";
    }
    this._config = Object.assign({}, rtcConfig);

    if (!rtcConfig.iceServers ||
        !Services.prefs.getBoolPref("media.peerconnection.use_document_iceservers")) {
      try {
         rtcConfig.iceServers =
           JSON.parse(Services.prefs.getCharPref("media.peerconnection.default_iceservers") || "[]");
      } catch (e) {
        this.logWarning(
            "Ignoring invalid media.peerconnection.default_iceservers in about:config");
        rtcConfig.iceServers = [];
      }
      try {
        this._mustValidateRTCConfiguration(rtcConfig,
            "Ignoring invalid media.peerconnection.default_iceservers in about:config");
      } catch (e) {
        this.logWarning(e.message);
        rtcConfig.iceServers = [];
      }
    } else {
      // This gets executed in the typical case when iceServers
      // are passed in through the web page.
      this._mustValidateRTCConfiguration(rtcConfig,
        "RTCPeerConnection constructor passed invalid RTCConfiguration");
    }
    var principal = Cu.getWebIDLCallerPrincipal();
    this._isChrome = Services.scriptSecurityManager.isSystemPrincipal(principal);

    if (_globalPCList._networkdown) {
      throw new this._win.DOMException(
          "Can't create RTCPeerConnections when the network is down",
          "InvalidStateError");
    }

    this.makeGetterSetterEH("ontrack");
    this.makeLegacyGetterSetterEH("onaddstream", "Use peerConnection.ontrack instead.");
    this.makeLegacyGetterSetterEH("onaddtrack", "Use peerConnection.ontrack instead.");
    this.makeGetterSetterEH("onicecandidate");
    this.makeGetterSetterEH("onnegotiationneeded");
    this.makeGetterSetterEH("onsignalingstatechange");
    this.makeGetterSetterEH("onremovestream");
    this.makeGetterSetterEH("ondatachannel");
    this.makeGetterSetterEH("oniceconnectionstatechange");
    this.makeGetterSetterEH("onicegatheringstatechange");
    this.makeGetterSetterEH("onidentityresult");
    this.makeGetterSetterEH("onpeeridentity");
    this.makeGetterSetterEH("onidpassertionerror");
    this.makeGetterSetterEH("onidpvalidationerror");

    this._pc = new this._win.PeerConnectionImpl();
    this._operationsChain = this._win.Promise.resolve();

    this.__DOM_IMPL__._innerObject = this;
    this._observer = new this._win.PeerConnectionObserver(this.__DOM_IMPL__);

    var location = "" + this._win.location;

    // Warn just once per PeerConnection about deprecated getStats usage.
    this._warnDeprecatedStatsAccessNullable = { warn: () =>
      this.logWarning("non-maplike pc.getStats access is deprecated, and will be removed in the near future! " +
                      "See http://w3c.github.io/webrtc-pc/#getstats-example for usage.") };

    this._warnDeprecatedStatsCallbacksNullable = { warn: () =>
      this.logWarning("Callback-based pc.getStats is deprecated, and will be removed in the near future! Use promise-version! " +
                      "See http://w3c.github.io/webrtc-pc/#getstats-example for usage.") };

    // Add a reference to the PeerConnection to global list (before init).
    _globalPCList.addPC(this);

    this._impl.initialize(this._observer, this._win, rtcConfig,
                          Services.tm.currentThread);

    this._certificateReady = this._initCertificate(rtcConfig.certificates);
    this._initIdp();
    _globalPCList.notifyLifecycleObservers(this, "initialized");
  }

  get _impl() {
    if (!this._pc) {
      throw new this._win.DOMException(
          "RTCPeerConnection is gone (did you enter Offline mode?)",
          "InvalidStateError");
    }
    return this._pc;
  }

  getConfiguration() {
    return this._config;
  }

  async _initCertificate(certificates = []) {
    let certificate;
    if (certificates.length > 1) {
      throw new this._win.DOMException(
        "RTCPeerConnection does not currently support multiple certificates",
        "NotSupportedError");
    }
    if (certificates.length) {
      certificate = certificates.find(c => c.expires > Date.now());
      if (!certificate) {
        throw new this._win.DOMException(
          "Unable to create RTCPeerConnection with an expired certificate",
          "InvalidParameterError");
      }
    }

    if (!certificate) {
      certificate = await this._win.RTCPeerConnection.generateCertificate({
        name: "ECDSA", namedCurve: "P-256"
      });
    }
    this._impl.certificate = certificate;
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

  async _chain(func) {
    let p = (async () => {
      await this._operationsChain;
      // Don't _checkClosed() inside the chain, because it throws, and spec
      // behavior is to NOT reject outstanding promises on close. This is what
      // happens most of the time anyways, as the c++ code stops calling us once
      // closed, hanging the chain. However, c++ may already have queued tasks
      // on us, so if we're one of those then sit back.
      if (this._closed) {
        return;
      }
      return await func();
    })();
    // don't propagate errors in the operations chain (this is a fork of p).
    this._operationsChain = p.catch(() => {});
    return await p;
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
    return (typeof onSucc == "function") ? this._legacy(onSucc, onErr, func)
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

    let ios = Cc['@mozilla.org/network/io-service;1'].getService(Ci.nsIIOService);

    let nicerNewURI = uriStr => {
      try {
        return ios.newURI(uriStr);
      } catch (e if (e.result == Cr.NS_ERROR_MALFORMED_URI)) {
        throw new this._win.DOMException(msg + " - malformed URI: " + uriStr,
                                         "SyntaxError");
      }
    };

    var stunServers = 0;

    iceServers.forEach(({ urls, username, credential, credentialType }) => {
      if (!urls) {
        throw new this._win.DOMException(msg + " - missing urls", "InvalidAccessError");
      }
      urls.map(url => nicerNewURI(url)).forEach(({ scheme, spec }) => {
        if (scheme in { turn:1, turns:1 }) {
          if (username == undefined) {
            throw new this._win.DOMException(msg + " - missing username: " + spec,
                                             "InvalidAccessError");
          }
          if (credential == undefined) {
            throw new this._win.DOMException(msg + " - missing credential: " + spec,
                                             "InvalidAccessError");
          }
          if (credentialType != "password") {
            this.logWarning("RTCConfiguration TURN credentialType \""+
                            credentialType +
                            "\" is not yet implemented. Treating as password."+
                            " https://bugzil.la/1247616");
          }
          this._hasTurnServer = true;
          stunServers += 1;
        } else if (scheme in { stun:1, stuns:1 }) {
          this._hasStunServer = true;
          stunServers += 1;
        } else {
          throw new this._win.DOMException(msg + " - improper scheme: " + scheme,
                                           "SyntaxError");
        }
        if (scheme in { stuns:1, turns:1 }) {
          this.logWarning(scheme.toUpperCase() + " is not yet supported.");
        }
        if (stunServers >= 5) {
          this.logError("Using five or more STUN/TURN servers causes problems");
        } else if (stunServers > 2) {
          this.logWarning("Using more than two STUN/TURN servers slows down discovery");
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
      throw new this._win.DOMException("Peer connection is closed",
                                       "InvalidStateError");
    }
  }

  dispatchEvent(event) {
    // PC can close while events are firing if there is an async dispatch
    // in c++ land. But let through "closed" signaling and ice connection events.
    if (!this._closed || this._inClose) {
      this.__DOM_IMPL__.dispatchEvent(event);
    }
  }

  // Log error message to web console and window.onerror, if present.
  logErrorAndCallOnError(e) {
    this.logMsg(e.message, e.fileName, e.lineNumber, Ci.nsIScriptError.exceptionFlag);

    // Safely call onerror directly if present (necessary for testing)
    try {
      if (typeof this._win.onerror === "function") {
        this._win.onerror(e.message, e.fileName, e.lineNumber);
      }
    } catch(e) {
      // If onerror itself throws, service it.
      try {
        this.logMsg(e.message, e.fileName, e.lineNumber, Ci.nsIScriptError.errorFlag);
      } catch(e) {}
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
    Object.defineProperty(this, name,
                          {
                            get:function()  { return this.getEH(name); },
                            set:function(h) { return this.setEH(name, h); }
                          });
  }

  makeLegacyGetterSetterEH(name, msg) {
    Object.defineProperty(this, name,
                          {
                            get:function()  { return this.getEH(name); },
                            set:function(h) {
                              this.logWarning(name + " is deprecated! " + msg);
                              return this.setEH(name, h);
                            }
                          });
  }

  createOffer(optionsOrOnSucc, onErr, options) {
    // This entry-point handles both new and legacy call sig. Decipher which one
    if (typeof optionsOrOnSucc == "function") {
      return this._legacy(optionsOrOnSucc, onErr, () => this._createOffer(options));
    }
    return this._async(() => this._createOffer(optionsOrOnSucc));
  }

  async _createOffer(options) {
    this._checkClosed();
    let origin = Cu.getWebIDLCallerPrincipal().origin;
    return await this._chain(async () => {
      let haveAssertion;
      if (this._localIdp.enabled) {
        haveAssertion = this._getIdentityAssertion(origin);
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
    });
  }

  createAnswer(optionsOrOnSucc, onErr) {
    // This entry-point handles both new and legacy call sig. Decipher which one
    if (typeof optionsOrOnSucc == "function") {
      return this._legacy(optionsOrOnSucc, onErr, () => this._createAnswer({}));
    }
    return this._async(() => this._createAnswer(optionsOrOnSucc));
  }

  async _createAnswer(options) {
    this._checkClosed();
    let origin = Cu.getWebIDLCallerPrincipal().origin;
    return await this._chain(async () => {
      // We give up line-numbers in errors by doing this here, but do all
      // state-checks inside the chain, to support the legacy feature that
      // callers don't have to wait for setRemoteDescription to finish.
      if (!this.remoteDescription) {
        throw new this._win.DOMException("setRemoteDescription not called",
                                         "InvalidStateError");
      }
      if (this.remoteDescription.type != "offer") {
        throw new this._win.DOMException("No outstanding offer",
                                         "InvalidStateError");
      }
      let haveAssertion;
      if (this._localIdp.enabled) {
        haveAssertion = this._getIdentityAssertion(origin);
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
    });
  }

  async _getPermission() {
    if (!this._havePermission) {
      let privileged = this._isChrome || AppConstants.MOZ_B2G ||
          Services.prefs.getBoolPref("media.navigator.permission.disabled");

      if (privileged) {
        this._havePermission = Promise.resolve();
      } else {
        this._havePermission = new Promise((resolve, reject) => {
          this._settlePermission = { allow: resolve, deny: reject };
          let outerId = this._win.QueryInterface(Ci.nsIInterfaceRequestor).
              getInterface(Ci.nsIDOMWindowUtils).outerWindowID;

          let chrome = new CreateOfferRequest(outerId, this._winID,
                                              this._globalPCListId, false);
          let request = this._win.CreateOfferRequest._create(this._win, chrome);
          Services.obs.notifyObservers(request, "PeerConnection:request", null);
        });
      }
    }
    return await this._havePermission;
  }

  setLocalDescription(desc, onSucc, onErr) {
    return this._auto(onSucc, onErr, () => this._setLocalDescription(desc));
  }

  async _setLocalDescription({ type, sdp }) {
    this._checkClosed();

    this._localType = type;

    let action = this._actions[type];
    if (action === undefined) {
      throw new this._win.DOMException(
          "Invalid type " + type + " provided to setLocalDescription",
          "InvalidParameterError");
    }
    if (action == Ci.IPeerConnection.kActionPRAnswer) {
      throw new this._win.DOMException("pranswer not yet implemented",
                                       "NotSupportedError");
    }

    if (!sdp && action != Ci.IPeerConnection.kActionRollback) {
      throw new this._win.DOMException(
          "Empty or null SDP provided to setLocalDescription",
          "InvalidParameterError");
    }

    return await this._chain(async () => {
      await this._getPermission();
      await new Promise((resolve, reject) => {
        this._onSetLocalDescriptionSuccess = resolve;
        this._onSetLocalDescriptionFailure = reject;
        this._impl.setLocalDescription(action, sdp);
      });
    });
  }

  async _validateIdentity(sdp, origin) {
    let expectedIdentity;

    // Only run a single identity verification at a time.  We have to do this to
    // avoid problems with the fact that identity validation doesn't block the
    // resolution of setRemoteDescription().
    let p = (async () => {
      try {
        await this._lastIdentityValidation;
        let msg = await this._remoteIdp.verifyIdentityFromSDP(sdp, origin);
        expectedIdentity = this._impl.peerIdentity;
        // If this pc has an identity already, then the identity in sdp must match
        if (expectedIdentity && (!msg || msg.identity !== expectedIdentity)) {
          this.close();
          throw new this._win.DOMException(
            "Peer Identity mismatch, expected: " + expectedIdentity,
            "IncompatibleSessionDescriptionError");
        }
        if (msg) {
          // Set new identity and generate an event.
          this._impl.peerIdentity = msg.identity;
          this._resolvePeerIdentity(Cu.cloneInto({
            idp: this._remoteIdp.provider,
            name: msg.identity
          }, this._win));
        }
      } catch(e) {
        this._rejectPeerIdentity(e);
        // If we don't expect a specific peer identity, failure to get a valid
        // peer identity is not a terminal state, so replace the promise to
        // allow another attempt.
        if (!this._impl.peerIdentity) {
          this._resetPeerIdentityPromise();
        }
        throw e;
      }
    })();
    this._lastIdentityValidation = p.catch(() => {});

    // Only wait for IdP validation if we need identity matching
    if (expectedIdentity) {
      await p;
    }
  }

  setRemoteDescription(desc, onSucc, onErr) {
    return this._auto(onSucc, onErr, () => this._setRemoteDescription(desc));
  }

  async _setRemoteDescription({ type, sdp }) {
    this._checkClosed();
    this._remoteType = type;

    let action = this._actions[type];
    if (action === undefined) {
      throw new this._win.DOMException(
          "Invalid type " + type + " provided to setRemoteDescription",
          "InvalidParameterError");
    }
    if (action == Ci.IPeerConnection.kActionPRAnswer) {
      throw new this._win.DOMException("pranswer not yet implemented",
                                       "NotSupportedError");
    }

    if (!sdp && type != "rollback") {
      throw new this._win.DOMException(
          "Empty or null SDP provided to setRemoteDescription",
          "InvalidParameterError");
    }

    // Get caller's origin before hitting the promise chain
    let origin = Cu.getWebIDLCallerPrincipal().origin;

    return await this._chain(async () => {
      let haveSetRemote = (async () => {
        await this._getPermission();
        await new Promise((resolve, reject) => {
          this._onSetRemoteDescriptionSuccess = resolve;
          this._onSetRemoteDescriptionFailure = reject;
          this._impl.setRemoteDescription(action, sdp);
        });
        this._updateCanTrickle();
      })();

      if (action != Ci.IPeerConnection.kActionRollback) {
        // Do setRemoteDescription and identity validation in parallel
        await this._validateIdentity(sdp, origin);
      }
      await haveSetRemote;
    });
  }

  setIdentityProvider(provider, protocol, username) {
    this._checkClosed();
    this._localIdp.setIdentityProvider(provider, protocol, username);
  }

  async _getIdentityAssertion(origin) {
    await this._certificateReady;
    return await this._localIdp.getIdentityAssertion(this._impl.fingerprint, origin);
  }

  getIdentityAssertion() {
    this._checkClosed();
    let origin = Cu.getWebIDLCallerPrincipal().origin;
    return this._win.Promise.resolve(this._chain(() =>
        this._getIdentityAssertion(origin)));
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

  // TODO: Implement processing for end-of-candidates (bug 1318167)
  addIceCandidate(cand, onSucc, onErr) {
    return this._auto(onSucc, onErr, () => cand && this._addIceCandidate(cand));
  }

  async _addIceCandidate({ candidate, sdpMid, sdpMLineIndex }) {
    this._checkClosed();
    if (sdpMid === null && sdpMLineIndex === null) {
      throw new this._win.DOMException(
          "Invalid candidate (both sdpMid and sdpMLineIndex are null).",
          "TypeError");
    }
    return await this._chain(() => new Promise((resolve, reject) => {
      this._onAddIceCandidateSuccess = resolve;
      this._onAddIceCandidateError = reject;
      this._impl.addIceCandidate(candidate, sdpMid || "", sdpMLineIndex);
    }));
  }

  addStream(stream) {
    stream.getTracks().forEach(track => this.addTrack(track, stream));
  }

  addTrack(track, stream) {
    if (stream.currentTime === undefined) {
      throw new this._win.DOMException("invalid stream.", "InvalidParameterError");
    }
    this._checkClosed();
    this._senders.forEach(sender => {
      if (sender.track == track) {
        throw new this._win.DOMException("already added.",
                                         "InvalidParameterError");
      }
    });
    this._impl.addTrack(track, stream);
    let sender = this._win.RTCRtpSender._create(this._win,
                                                new RTCRtpSender(this, track,
                                                                 stream));
    this._senders.push(sender);
    return sender;
  }

  removeTrack(sender) {
    this._checkClosed();
    var i = this._senders.indexOf(sender);
    if (i >= 0) {
      this._senders.splice(i, 1);
      this._impl.removeTrack(sender.track); // fires negotiation needed
    }
  }

  _insertDTMF(sender, tones, duration, interToneGap) {
    return this._impl.insertDTMF(sender.__DOM_IMPL__, tones, duration, interToneGap);
  }

  _getDTMFToneBuffer(sender) {
    return this._impl.getDTMFToneBuffer(sender.__DOM_IMPL__);
  }

  async _replaceTrack(sender, withTrack) {
    this._checkClosed();
    return await this._chain(() => new Promise((resolve, reject) => {
      this._onReplaceTrackSender = sender;
      this._onReplaceTrackWithTrack = withTrack;
      this._onReplaceTrackSuccess = resolve;
      this._onReplaceTrackFailure = reject;
      this._impl.replaceTrack(sender.track, withTrack);
    }));
  }

  _setParameters({ track }, parameters) {
    if (!Services.prefs.getBoolPref("media.peerconnection.simulcast")) {
      return;
    }
    // validate parameters input
    var encodings = parameters.encodings || [];

    encodings.reduce((uniqueRids, { rid, scaleResolutionDownBy }) => {
      if (scaleResolutionDownBy < 1.0) {
        throw new this._win.RangeError("scaleResolutionDownBy must be >= 1.0");
      }
      if (!rid && encodings.length > 1) {
        throw new this._win.DOMException("Missing rid", "TypeError");
      }
      if (uniqueRids[rid]) {
        throw new this._win.DOMException("Duplicate rid", "TypeError");
      }
      uniqueRids[rid] = true;
      return uniqueRids;
    }, {});

    this._impl.setParameters(track, parameters);
  }

  _getParameters({ track }) {
    if (!Services.prefs.getBoolPref("media.peerconnection.simulcast")) {
      return;
    }
    return this._impl.getParameters(track);
  }

  close() {
    if (this._closed) {
      return;
    }
    this._closed = true;
    this._inClose = true;
    this.changeIceConnectionState("closed");
    this._localIdp.close();
    this._remoteIdp.close();
    this._impl.close();
    this._inClose = false;
  }

  getLocalStreams() {
    this._checkClosed();
    return this._impl.getLocalStreams();
  }

  getRemoteStreams() {
    this._checkClosed();
    return this._impl.getRemoteStreams();
  }

  getSenders() {
    return this._senders;
  }

  getReceivers() {
    return this._receivers;
  }

  mozSelectSsrc(receiver, ssrcIndex) {
    this._impl.selectSsrc(receiver.track, ssrcIndex);
  }

  get localDescription() {
    this._checkClosed();
    let sdp = this._impl.localDescription;
    if (sdp.length == 0) {
      return null;
    }
    return new this._win.RTCSessionDescription({ type: this._localType, sdp });
  }

  get remoteDescription() {
    this._checkClosed();
    let sdp = this._impl.remoteDescription;
    if (sdp.length == 0) {
      return null;
    }
    return new this._win.RTCSessionDescription({ type: this._remoteType, sdp });
  }

  get peerIdentity() { return this._peerIdentity; }
  get idpLoginUrl() { return this._localIdp.idpLoginUrl; }
  get id() { return this._impl.id; }
  set id(s) { this._impl.id = s; }
  get iceGatheringState()  { return this._iceGatheringState; }
  get iceConnectionState() { return this._iceConnectionState; }

  get signalingState() {
    // checking for our local pc closed indication
    // before invoking the pc methods.
    if (this._closed) {
      return "closed";
    }
    return {
      "SignalingInvalid":            "",
      "SignalingStable":             "stable",
      "SignalingHaveLocalOffer":     "have-local-offer",
      "SignalingHaveRemoteOffer":    "have-remote-offer",
      "SignalingHaveLocalPranswer":  "have-local-pranswer",
      "SignalingHaveRemotePranswer": "have-remote-pranswer",
      "SignalingClosed":             "closed"
    }[this._impl.signalingState];
  }

  changeIceGatheringState(state) {
    this._iceGatheringState = state;
    _globalPCList.notifyLifecycleObservers(this, "icegatheringstatechange");
    this.dispatchEvent(new this._win.Event("icegatheringstatechange"));
  }

  changeIceConnectionState(state) {
    if (state != this._iceConnectionState) {
      this._iceConnectionState = state;
      _globalPCList.notifyLifecycleObservers(this, "iceconnectionstatechange");
      this.dispatchEvent(new this._win.Event("iceconnectionstatechange"));
    }
  }

  getStats(selector, onSucc, onErr) {
    let isLegacy = (typeof onSucc) == "function";
    if (isLegacy &&
        this._warnDeprecatedStatsCallbacksNullable.warn) {
      this._warnDeprecatedStatsCallbacksNullable.warn();
      this._warnDeprecatedStatsCallbacksNullable.warn = null;
    }
    return this._auto(onSucc, onErr, () => this._getStats(selector, isLegacy));
  }

  async _getStats(selector, isLegacy) {
    // getStats is allowed even in closed state.
    return await this._chain(() => new Promise((resolve, reject) => {
      this._onGetStatsIsLegacy = isLegacy;
      this._onGetStatsSuccess = resolve;
      this._onGetStatsFailure = reject;
      this._impl.getStats(selector);
    }));
  }

  createDataChannel(label, {
                      maxRetransmits, ordered, negotiated, id = 0xFFFF,
                      maxRetransmitTime, maxPacketLifeTime = maxRetransmitTime,
                      protocol
                    } = {}) {
    this._checkClosed();

    if (maxRetransmitTime !== undefined) {
      this.logWarning("Use maxPacketLifeTime instead of deprecated maxRetransmitTime which will stop working soon in createDataChannel!");
    }
    if (maxPacketLifeTime !== undefined && maxRetransmits !== undefined) {
      throw new this._win.DOMException(
          "Both maxPacketLifeTime and maxRetransmits cannot be provided",
          "InvalidParameterError");
    }
    // Must determine the type where we still know if entries are undefined.
    let type;
    if (maxPacketLifeTime) {
      type = Ci.IPeerConnection.kDataChannelPartialReliableTimed;
    } else if (maxRetransmits) {
      type = Ci.IPeerConnection.kDataChannelPartialReliableRexmit;
    } else {
      type = Ci.IPeerConnection.kDataChannelReliable;
    }
    // Synchronous since it doesn't block.
    return this._impl.createDataChannel(label, protocol, type, ordered,
                                        maxPacketLifeTime, maxRetransmits,
                                        negotiated, id);
  }
}
setupPrototype(RTCPeerConnection, {
  classID: PC_CID,
  contractID: PC_CONTRACT,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer]),
  _actions: {
    offer: Ci.IPeerConnection.kActionOffer,
    answer: Ci.IPeerConnection.kActionAnswer,
    pranswer: Ci.IPeerConnection.kActionPRAnswer,
    rollback: Ci.IPeerConnection.kActionRollback,
    answer: Ci.IPeerConnection.kActionAnswer,
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

  newError(message, code) {
    // These strings must match those defined in the WebRTC spec.
    const reasonName = [
      "",
      "InternalError",
      "InvalidCandidateError",
      "InvalidParameterError",
      "InvalidStateError",
      "InvalidSessionDescriptionError",
      "IncompatibleSessionDescriptionError",
      "InternalError",
      "IncompatibleMediaStreamTrackError",
      "InternalError"
    ];
    let name = reasonName[Math.min(code, reasonName.length - 1)];
    return new this._dompc._win.DOMException(message, name);
  }

  dispatchEvent(event) {
    this._dompc.dispatchEvent(event);
  }

  onCreateOfferSuccess(sdp) {
    this._dompc._onCreateOfferSuccess(sdp);
  }

  onCreateOfferError(code, message) {
    this._dompc._onCreateOfferFailure(this.newError(message, code));
  }

  onCreateAnswerSuccess(sdp) {
    this._dompc._onCreateAnswerSuccess(sdp);
  }

  onCreateAnswerError(code, message) {
    this._dompc._onCreateAnswerFailure(this.newError(message, code));
  }

  onSetLocalDescriptionSuccess() {
    this._dompc._onSetLocalDescriptionSuccess();
  }

  onSetRemoteDescriptionSuccess() {
    this._dompc._onSetRemoteDescriptionSuccess();
  }

  onSetLocalDescriptionError(code, message) {
    this._localType = null;
    this._dompc._onSetLocalDescriptionFailure(this.newError(message, code));
  }

  onSetRemoteDescriptionError(code, message) {
    this._remoteType = null;
    this._dompc._onSetRemoteDescriptionFailure(this.newError(message, code));
  }

  onAddIceCandidateSuccess() {
    this._dompc._onAddIceCandidateSuccess();
  }

  onAddIceCandidateError(code, message) {
    this._dompc._onAddIceCandidateError(this.newError(message, code));
  }

  onIceCandidate(sdpMLineIndex, sdpMid, candidate) {
    let win = this._dompc._win;
    if (candidate) {
      if (candidate.includes(" typ relay ")) {
        this._dompc._iceGatheredRelayCandidates = true;
      }
      candidate = new win.RTCIceCandidate({ candidate, sdpMid, sdpMLineIndex });
    } else {
      candidate = null;

    }
    this.dispatchEvent(new win.RTCPeerConnectionIceEvent("icecandidate",
                                                         { candidate }));
  }

  onNegotiationNeeded() {
    this.dispatchEvent(new this._win.Event("negotiationneeded"));
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
    if (pc.iceConnectionState === 'new') {
      var checking_histogram = Services.telemetry.getHistogramById("WEBRTC_ICE_CHECKING_RATE");
      if (iceConnectionState === 'checking') {
        checking_histogram.add(true);
      } else if (iceConnectionState === 'failed') {
        checking_histogram.add(false);
      }
    } else if (pc.iceConnectionState === 'checking') {
      var success_histogram = Services.telemetry.getHistogramById("WEBRTC_ICE_SUCCESS_RATE");
      if (iceConnectionState === 'completed' ||
          iceConnectionState === 'connected') {
        success_histogram.add(true);
      } else if (iceConnectionState === 'failed') {
        success_histogram.add(false);
      }
    }

    if (iceConnectionState === 'failed') {
      if (!pc._hasStunServer) {
        pc.logError("ICE failed, add a STUN server and see about:webrtc for more details");
      }
      else if (!pc._hasTurnServer) {
        pc.logError("ICE failed, add a TURN server and see about:webrtc for more details");
      }
      else if (pc._hasTurnServer && !pc._iceGatheredRelayCandidates) {
        pc.logError("ICE failed, your TURN server appears to be broken, see about:webrtc for more details");
      }
      else {
        pc.logError("ICE failed, see about:webrtc for more details");
      }
    }

    pc.changeIceConnectionState(iceConnectionState);
  }

  // This method is responsible for updating iceGatheringState. This
  // state is defined in the WebRTC specification as follows:
  //
  // iceGatheringState:
  // ------------------
  //   new        The object was just created, and no networking has occurred
  //              yet.
  //
  //   gathering  The ICE agent is in the process of gathering candidates for
  //              this RTCPeerConnection.
  //
  //   complete   The ICE agent has completed gathering. Events such as adding
  //              a new interface or a new TURN server will cause the state to
  //              go back to gathering.
  //
  handleIceGatheringStateChange(gatheringState) {
    let pc = this._dompc;
    if (pc.iceGatheringState === gatheringState) {
      return;
    }
    pc.changeIceGatheringState(gatheringState);
  }

  onStateChange(state) {
    switch (state) {
      case "SignalingState":
        this.dispatchEvent(new this._win.Event("signalingstatechange"));
        break;

      case "IceConnectionState":
        this.handleIceConnectionStateChange(this._dompc._pc.iceConnectionState);
        break;

      case "IceGatheringState":
        this.handleIceGatheringStateChange(this._dompc._pc.iceGatheringState);
        break;

      case "SdpState":
        // No-op
        break;

      case "ReadyState":
        // No-op
        break;

      case "SipccState":
        // No-op
        break;

      default:
        this._dompc.logWarning("Unhandled state type: " + state);
        break;
    }
  }

  onGetStatsSuccess(dict) {
    let pc = this._dompc;
    let chromeobj = new RTCStatsReport(pc._win, dict);
    let webidlobj = pc._win.RTCStatsReport._create(pc._win, chromeobj);
    chromeobj.makeStatsPublic(pc._warnDeprecatedStatsCallbacksNullable &&
                              pc._warnDeprecatedStatsAccessNullable,
                              pc._onGetStatsIsLegacy);
    pc._onGetStatsSuccess(webidlobj);
  }

  onGetStatsError(code, message) {
    this._dompc._onGetStatsFailure(this.newError(message, code));
  }

  onAddStream(stream) {
    let ev = new this._dompc._win.MediaStreamEvent("addstream", { stream });
    this.dispatchEvent(ev);
  }

  onRemoveStream(stream) {
    this.dispatchEvent(new this._dompc._win.MediaStreamEvent("removestream",
                                                             { stream }));
  }

  onAddTrack(track, streams) {
    let pc = this._dompc;
    let receiver = pc._win.RTCRtpReceiver._create(pc._win,
                                                  new RTCRtpReceiver(this,
                                                                     track));
    pc._receivers.push(receiver);
    let ev = new pc._win.RTCTrackEvent("track", { receiver, track, streams });
    this.dispatchEvent(ev);

    // Fire legacy event as well for a little bit.
    ev = new pc._win.MediaStreamTrackEvent("addtrack", { track });
    this.dispatchEvent(ev);
  }

  onRemoveTrack(track) {
    let pc = this._dompc;
    let i = pc._receivers.findIndex(receiver => receiver.track == track);
    if (i >= 0) {
      pc._receivers.splice(i, 1);
    }
  }

  onReplaceTrackSuccess() {
    var pc = this._dompc;
    pc._onReplaceTrackSender.track = pc._onReplaceTrackWithTrack;
    pc._onReplaceTrackWithTrack = null;
    pc._onReplaceTrackSender = null;
    pc._onReplaceTrackSuccess();
  }

  onReplaceTrackError(code, message) {
    var pc = this._dompc;
    pc._onReplaceTrackWithTrack = null;
    pc._onReplaceTrackSender = null;
    pc._onReplaceTrackFailure(this.newError(message, code));
  }

  notifyDataChannel(channel) {
    this.dispatchEvent(new this._dompc._win.RTCDataChannelEvent("datachannel",
                                                                { channel }));
  }

  onDTMFToneChange(trackId, tone) {
    var pc = this._dompc;
    var sender = pc._senders.find(({track}) => track.id == trackId);
    sender.dtmf.dispatchEvent(new pc._win.RTCDTMFToneChangeEvent("tonechange",
                                                                 { tone }));
  }
}
setupPrototype(PeerConnectionObserver, {
  classID: PC_OBS_CID,
  contractID: PC_OBS_CONTRACT,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer])
});

class RTCPeerConnectionStatic {
  init(win) {
    this._winID = win.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;
  }

  registerPeerConnectionLifecycleCallback(cb) {
    _globalPCList._registerPeerConnectionLifecycleCallback(this._winID, cb);
  }
}
setupPrototype(RTCPeerConnectionStatic, {
  classID: PC_STATIC_CID,
  contractID: PC_STATIC_CONTRACT,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer])
});

class RTCDTMFSender {
  constructor(sender) {
    this._sender = sender;
  }

  get toneBuffer() {
    return this._sender._pc._getDTMFToneBuffer(this._sender);
  }

  get ontonechange() {
    return this.__DOM_IMPL__.getEventHandler("ontonechange");
  }

  set ontonechange(handler) {
    this.__DOM_IMPL__.setEventHandler("ontonechange", handler);
  }

  insertDTMF(tones, duration, interToneGap) {
    this._sender._pc._checkClosed();

    if (this._sender._pc._senders.indexOf(this._sender.__DOM_IMPL__) == -1) {
      throw new this._sender._pc._win.DOMException("RTCRtpSender is stopped",
                                                   "InvalidStateError");
    }

    duration = Math.max(40, Math.min(duration, 6000));
    if (interToneGap < 30) interToneGap = 30;

    tones = tones.toUpperCase();

    if (tones.match(/[^0-9A-D#*,]/)) {
      throw new this._sender._pc._win.DOMException("Invalid DTMF characters",
                                                   "InvalidCharacterError");
    }

    this._sender._pc._insertDTMF(this._sender, tones, duration, interToneGap);
  }
}
setupPrototype(RTCDTMFSender, {
  classID: PC_DTMF_SENDER_CID,
  contractID: PC_DTMF_SENDER_CONTRACT,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports])
});

class RTCRtpSender {
  constructor(pc, track, stream) {
    let dtmf = pc._win.RTCDTMFSender._create(pc._win, new RTCDTMFSender(this));
    Object.assign(this, { _pc: pc, track, _stream: stream, dtmf });
  }

  replaceTrack(withTrack) {
    return this._pc._async(() => this._pc._replaceTrack(this, withTrack));
  }

  setParameters(parameters) {
    return this._pc._win.Promise.resolve()
      .then(() => this._pc._setParameters(this, parameters));
  }

  getParameters() {
    return this._pc._getParameters(this);
  }
}
setupPrototype(RTCRtpSender, {
  classID: PC_SENDER_CID,
  contractID: PC_SENDER_CONTRACT,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports])
});

class RTCRtpReceiver {
  constructor(pc, track) {
    Object.assign(this, { _pc: pc, track });
  }
}
setupPrototype(RTCRtpReceiver, {
  classID: PC_RECEIVER_CID,
  contractID: PC_RECEIVER_CONTRACT,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports])
});

class CreateOfferRequest {
  constructor(windowID, innerWindowID, callID, isSecure) {
    Object.assign(this, { windowID, innerWindowID, callID, isSecure });
  }
}
setupPrototype(CreateOfferRequest, {
  classID: PC_COREQUEST_CID,
  contractID: PC_COREQUEST_CONTRACT,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports])
});

this.NSGetFactory = XPCOMUtils.generateNSGetFactory(
  [GlobalPCList,
   RTCDTMFSender,
   RTCIceCandidate,
   RTCSessionDescription,
   RTCPeerConnection,
   RTCPeerConnectionStatic,
   RTCRtpReceiver,
   RTCRtpSender,
   RTCStatsReport,
   PeerConnectionObserver,
   CreateOfferRequest]
);
