/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
const PREF_FXA_ENABLED = "identity.fxaccounts.enabled";
let _fxa_enabled = false;
try {
  if (Services.prefs.getPrefType(PREF_FXA_ENABLED) === Ci.nsIPrefBranch.PREF_BOOL) {
    _fxa_enabled = Services.prefs.getBoolPref(PREF_FXA_ENABLED);
  }
} catch(noPref) {
}
const FXA_ENABLED = _fxa_enabled;

// This is the parent process corresponding to nsDOMIdentity.
this.EXPORTED_SYMBOLS = ["DOMIdentity"];

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "objectCopy",
                                  "resource://gre/modules/identity/IdentityUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "IdentityService",
#ifdef MOZ_B2G_VERSION
                                  "resource://gre/modules/identity/MinimalIdentity.jsm");
#else
                                  "resource://gre/modules/identity/Identity.jsm");
#endif

XPCOMUtils.defineLazyModuleGetter(this, "FirefoxAccounts",
                                  "resource://gre/modules/identity/FirefoxAccounts.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "makeMessageObject",
                                  "resource://gre/modules/identity/IdentityUtils.jsm");

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
  }
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
  }
};

function RPWatchContext(aOptions, aTargetMM) {
  objectCopy(aOptions, this);

  // id and origin are required
  if (! (this.id && this.origin)) {
    throw new Error("id and origin are required for RP watch context");
  }

  // default for no loggedInUser is undefined, not null
  this.loggedInUser = aOptions.loggedInUser;

  // Maybe internal.  For hosted b2g identity shim.
  this._internal = aOptions._internal;

  // By default, set the audience of the assertion to the origin of the RP. Bug
  // 947374 will make it possible for certified apps and packaged apps on
  // FirefoxOS to request a different audience from their origin.
  //
  // For BrowserID on b2g, this audience value is consumed by a hosted identity
  // shim, set up by b2g/components/SignInToWebsite.jsm.
  this.audience = this.origin;

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

  doCancel: function RPWatchContext_oncancel() {
    log("doCancel: " + this.id);
    let message = new IDDOMMessage({id: this.id});
    this._mm.sendAsyncMessage("Identity:RP:Watch:OnCancel", message);
  },

  doError: function RPWatchContext_onerror(aMessage) {
    log("doError: " + aMessage);
  }
};

this.DOMIdentity = {
  /*
   * When relying parties (RPs) invoke the watch() method, they can request
   * to use Firefox Accounts as their auth service or BrowserID (the default).
   * For each RP, we create an RPWatchContext to store the parameters given to
   * watch(), and to provide hooks to invoke the onlogin(), onlogout(), etc.
   * callbacks held in the nsDOMIdentity state.
   *
   * The serviceContexts map associates the window ID of the RP with the
   * context object.  The mmContexts map associates a message manager with a
   * window ID.  We use the mmContexts map when child-process-shutdown is
   * observed, and all we have is a message manager to identify the window in
   * question.
   */
  _serviceContexts: new Map(),
  _mmContexts: new Map(),

  /*
   * Create a new RPWatchContext, and update the context maps.
   */
  newContext: function(message, targetMM) {
    let context = new RPWatchContext(message, targetMM);
    this._serviceContexts.set(message.id, context);
    this._mmContexts.set(targetMM, message.id);
    return context;
  },

  /*
   * Get the identity service used for an RP.
   *
   * @object message
   *         A message received from an RP.  Will include the id of the window
   *         whence the message originated.
   *
   * Returns FirefoxAccounts or IdentityService
   */
  getService: function(message) {
    if (!this._serviceContexts.has(message.id)) {
      throw new Error("getService called before newContext for " + message.id);
    }

    let context = this._serviceContexts.get(message.id);
    if (context.wantIssuer == "firefox-accounts") {
      if (FXA_ENABLED) {
        return FirefoxAccounts;
      }
      log("WARNING: Firefox Accounts is not enabled; Defaulting to BrowserID");
    }
    return IdentityService;
  },

  /*
   * Get the RPWatchContext object for a given message manager.
   */
  getContextForMM: function(targetMM) {
    return this._serviceContexts.get(this._mmContexts.get(targetMM));
  },

  /*
   * Delete the RPWatchContext object for a given message manager.  Removes the
   * mapping both from _serviceContexts and _mmContexts.
   */
  deleteContextForMM: function(targetMM) {
    this._serviceContexts.delete(this._mmContexts.get(targetMM));
    this._mmContexts.delete(targetMM);
  },

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
      case "Identity:RP:Unwatch":
        this._unwatch(msg, targetMM);
        break;
      case "Identity:RP:Request":
        this._request(msg, targetMM);
        break;
      case "Identity:RP:Logout":
        this._logout(msg, targetMM);
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
      case "child-process-shutdown":
        // we receive child-process-shutdown if the appliction crashes,
        // including if it is crashed by the OS (killed for out-of-memory,
        // for example)
        this._childProcessShutdown(targetMM);
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
             "Identity:IDP:AuthenticationFailure",
             "Identity:RP:Unwatch",
             "child-process-shutdown"],

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

  _watch: function DOMIdentity__watch(message, targetMM) {
    log("DOMIdentity__watch: " + message.id);
    let context = this.newContext(message, targetMM);
    this.getService(message).RP.watch(context);
  },

  _unwatch: function DOMIdentity_unwatch(message, targetMM) {
    this.getService(message).RP.unwatch(message.id, targetMM);
  },

  _request: function DOMIdentity__request(message) {
    this.getService(message).RP.request(message.id, message);
  },

  _logout: function DOMIdentity__logout(message) {
    log("logout " + message + "\n");
    this.getService(message).RP.logout(message.id, message.origin, message);
  },

  _childProcessShutdown: function DOMIdentity__childProcessShutdown(targetMM) {
    this.getContextForMM(targetMM).RP.childProcessShutdown(targetMM);
    this.deleteContextForMM(targetMM);

    let options = makeMessageObject({messageManager: targetMM, id: null, origin: null});
    Services.obs.notifyObservers({wrappedJSObject: options}, "identity-child-process-shutdown", null);
  },

  _beginProvisioning: function DOMIdentity__beginProvisioning(message, targetMM) {
    let context = new IDPProvisioningContext(message.id, message.origin,
                                             targetMM);
    this.getService(message).IDP.beginProvisioning(context);
  },

  _genKeyPair: function DOMIdentity__genKeyPair(message) {
    this.getService(message).IDP.genKeyPair(message.id);
  },

  _registerCertificate: function DOMIdentity__registerCertificate(message) {
    this.getService(message).IDP.registerCertificate(message.id, message.cert);
  },

  _provisioningFailure: function DOMIdentity__provisioningFailure(message) {
    this.getService(message).IDP.raiseProvisioningFailure(message.id, message.reason);
  },

  _beginAuthentication: function DOMIdentity__beginAuthentication(message, targetMM) {
    let context = new IDPAuthenticationContext(message.id, message.origin,
                                               targetMM);
    this.getService(message).IDP.beginAuthentication(context);
  },

  _completeAuthentication: function DOMIdentity__completeAuthentication(message) {
    this.getService(message).IDP.completeAuthentication(message.id);
  },

  _authenticationFailure: function DOMIdentity__authenticationFailure(message) {
    this.getService(message).IDP.cancelAuthentication(message.id);
  }
};

// Object is initialized by nsIDService.js
