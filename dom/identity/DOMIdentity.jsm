/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

// This is the parent process corresponding to nsDOMIdentity.
this.EXPORTED_SYMBOLS = ["DOMIdentity"];

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/identity/IdentityUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "IdentityService",
#ifdef MOZ_B2G_VERSION
                                  "resource://gre/modules/identity/MinimalIdentity.jsm");
#else
                                  "resource://gre/modules/identity/Identity.jsm");
#endif

XPCOMUtils.defineLazyModuleGetter(this,
                                  "Logger",
                                  "resource://gre/modules/identity/LogUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageListenerManager");

function log(...aMessageArgs) {
  Logger.log.apply(Logger, ["DOMIdentity"].concat(aMessageArgs));
}

function IDDOMMessage(aOptions) {
  objectCopy(aOptions, this);
}

function IDPProvisioningContext(aID, aOrigin, aTargetMM) {
  this._id = aID;
  this._origin = aOrigin;
  this._mm = aTargetMM;
}

IDPProvisioningContext.prototype = {
  get id() this._id,
  get origin() this._origin,

  doBeginProvisioningCallback: function IDPPC_doBeginProvCB(aID, aCertDuration) {
    let message = new IDDOMMessage({id: this.id});
    message.identity = aID;
    message.certDuration = aCertDuration;
    this._mm.sendAsyncMessage("Identity:IDP:CallBeginProvisioningCallback",
                              message);
  },

  doGenKeyPairCallback: function IDPPC_doGenKeyPairCallback(aPublicKey) {
    log("doGenKeyPairCallback");
    let message = new IDDOMMessage({id: this.id});
    message.publicKey = aPublicKey;
    this._mm.sendAsyncMessage("Identity:IDP:CallGenKeyPairCallback", message);
  },

  doError: function(msg) {
    log("Provisioning ERROR: " + msg);
  },
};

function IDPAuthenticationContext(aID, aOrigin, aTargetMM) {
  this._id = aID;
  this._origin = aOrigin;
  this._mm = aTargetMM;
}

IDPAuthenticationContext.prototype = {
  get id() this._id,
  get origin() this._origin,

  doBeginAuthenticationCallback: function IDPAC_doBeginAuthCB(aIdentity) {
    let message = new IDDOMMessage({id: this.id});
    message.identity = aIdentity;
    this._mm.sendAsyncMessage("Identity:IDP:CallBeginAuthenticationCallback",
                              message);
  },

  doError: function IDPAC_doError(msg) {
    log("Authentication ERROR: " + msg);
  },
};

function RPWatchContext(aOptions, aTargetMM) {
  objectCopy(aOptions, this);

  // id and origin are required
  if (! (this.id && this.origin)) {
    throw new Error("id and origin are required for RP watch context");
  }

  // default for no loggedInUser is undefined, not null
  this.loggedInUser = aOptions.loggedInUser;

  // Maybe internal
  this._internal = aOptions._internal;

  this._mm = aTargetMM;
}

RPWatchContext.prototype = {
  doLogin: function RPWatchContext_onlogin(aAssertion, aMaybeInternalParams) {
    log("doLogin: " + this.id);
    let message = new IDDOMMessage({id: this.id, assertion: aAssertion});
    if (aMaybeInternalParams) {
      message._internalParams = aMaybeInternalParams;
    }
    this._mm.sendAsyncMessage("Identity:RP:Watch:OnLogin", message);
  },

  doLogout: function RPWatchContext_onlogout() {
    log("doLogout: " + this.id);
    let message = new IDDOMMessage({id: this.id});
    this._mm.sendAsyncMessage("Identity:RP:Watch:OnLogout", message);
  },

  doReady: function RPWatchContext_onready() {
    log("doReady: " + this.id);
    let message = new IDDOMMessage({id: this.id});
    this._mm.sendAsyncMessage("Identity:RP:Watch:OnReady", message);
  },

  doError: function RPWatchContext_onerror(aMessage) {
    log("doError: " + aMessage);
  }
};

this.DOMIdentity = {
  // nsIMessageListener
  receiveMessage: function DOMIdentity_receiveMessage(aMessage) {
    let msg = aMessage.json;

    // Target is the frame message manager that called us and is
    // used to send replies back to the proper window.
    let targetMM = aMessage.target;

    switch (aMessage.name) {
      // RP
      case "Identity:RP:Watch":
        this._watch(msg, targetMM);
        break;
      case "Identity:RP:Request":
        this._request(msg);
        break;
      case "Identity:RP:Logout":
        this._logout(msg);
        break;
      // IDP
      case "Identity:IDP:BeginProvisioning":
        this._beginProvisioning(msg, targetMM);
        break;
      case "Identity:IDP:GenKeyPair":
        this._genKeyPair(msg);
        break;
      case "Identity:IDP:RegisterCertificate":
        this._registerCertificate(msg);
        break;
      case "Identity:IDP:ProvisioningFailure":
        this._provisioningFailure(msg);
        break;
      case "Identity:IDP:BeginAuthentication":
        this._beginAuthentication(msg, targetMM);
        break;
      case "Identity:IDP:CompleteAuthentication":
        this._completeAuthentication(msg);
        break;
      case "Identity:IDP:AuthenticationFailure":
        this._authenticationFailure(msg);
        break;
    }
  },

  // nsIObserver
  observe: function DOMIdentity_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "xpcom-shutdown":
        this._unsubscribeListeners();
        Services.obs.removeObserver(this, "xpcom-shutdown");
        Services.ww.unregisterNotification(this);
        break;
    }
  },

  messages: ["Identity:RP:Watch", "Identity:RP:Request", "Identity:RP:Logout",
             "Identity:IDP:BeginProvisioning", "Identity:IDP:ProvisioningFailure",
             "Identity:IDP:RegisterCertificate", "Identity:IDP:GenKeyPair",
             "Identity:IDP:BeginAuthentication",
             "Identity:IDP:CompleteAuthentication",
             "Identity:IDP:AuthenticationFailure"],

  // Private.
  _init: function DOMIdentity__init() {
    Services.ww.registerNotification(this);
    Services.obs.addObserver(this, "xpcom-shutdown", false);
    this._subscribeListeners();
  },

  _subscribeListeners: function DOMIdentity__subscribeListeners() {
    if (!ppmm) return;
    for (let message of this.messages) {
      ppmm.addMessageListener(message, this);
    }
  },

  _unsubscribeListeners: function DOMIdentity__unsubscribeListeners() {
    for (let message of this.messages) {
      ppmm.removeMessageListener(message, this);
    }
    ppmm = null;
  },

  _resetFrameState: function(aContext) {
    log("_resetFrameState: ", aContext.id);
    if (!aContext._mm) {
      throw new Error("ERROR: Trying to reset an invalid context");
    }
    let message = new IDDOMMessage({id: aContext.id});
    aContext._mm.sendAsyncMessage("Identity:ResetState", message);
  },

  _watch: function DOMIdentity__watch(message, targetMM) {
    log("DOMIdentity__watch: " + message.id);
    // Pass an object with the watch members to Identity.jsm so it can call the
    // callbacks.
    let context = new RPWatchContext(message, targetMM);
    IdentityService.RP.watch(context);
  },

  _request: function DOMIdentity__request(message) {
    IdentityService.RP.request(message.id, message);
  },

  _logout: function DOMIdentity__logout(message) {
    IdentityService.RP.logout(message.id, message.origin, message);
  },

  _beginProvisioning: function DOMIdentity__beginProvisioning(message, targetMM) {
    let context = new IDPProvisioningContext(message.id, message.origin,
                                             targetMM);
    IdentityService.IDP.beginProvisioning(context);
  },

  _genKeyPair: function DOMIdentity__genKeyPair(message) {
    IdentityService.IDP.genKeyPair(message.id);
  },

  _registerCertificate: function DOMIdentity__registerCertificate(message) {
    IdentityService.IDP.registerCertificate(message.id, message.cert);
  },

  _provisioningFailure: function DOMIdentity__provisioningFailure(message) {
    IdentityService.IDP.raiseProvisioningFailure(message.id, message.reason);
  },

  _beginAuthentication: function DOMIdentity__beginAuthentication(message, targetMM) {
    let context = new IDPAuthenticationContext(message.id, message.origin,
                                               targetMM);
    IdentityService.IDP.beginAuthentication(context);
  },

  _completeAuthentication: function DOMIdentity__completeAuthentication(message) {
    IdentityService.IDP.completeAuthentication(message.id);
  },

  _authenticationFailure: function DOMIdentity__authenticationFailure(message) {
    IdentityService.IDP.cancelAuthentication(message.id);
  },
};

// Object is initialized by nsIDService.js
