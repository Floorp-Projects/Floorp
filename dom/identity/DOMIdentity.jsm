/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc,
       interfaces: Ci,
       utils: Cu,
       results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const PREF_FXA_ENABLED = "identity.fxaccounts.enabled";
const FXA_PERMISSION = "firefox-accounts";

// This is the parent process corresponding to nsDOMIdentity.
this.EXPORTED_SYMBOLS = ["DOMIdentity"];

XPCOMUtils.defineLazyModuleGetter(this, "objectCopy",
                                  "resource://gre/modules/identity/IdentityUtils.jsm");

/* jshint ignore:start */
XPCOMUtils.defineLazyModuleGetter(this, "IdentityService",
#ifdef MOZ_B2G_VERSION
                                  "resource://gre/modules/identity/MinimalIdentity.jsm");
#else
                                  "resource://gre/modules/identity/Identity.jsm");
#endif
/* jshint ignore:end */

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

XPCOMUtils.defineLazyServiceGetter(this, "permissionManager",
                                   "@mozilla.org/permissionmanager;1",
                                   "nsIPermissionManager");

function log(...aMessageArgs) {
  Logger.log.apply(Logger, ["DOMIdentity"].concat(aMessageArgs));
}

function IDDOMMessage(aOptions) {
  objectCopy(aOptions, this);
}

function _sendAsyncMessage(identifier, message) {
  if (this._mm) {
    try {
      this._mm.sendAsyncMessage(identifier, message);
    } catch(err) {
      // We may receive a NS_ERROR_NOT_INITIALIZED if the target window has
      // been closed.  This can legitimately happen if an app has been killed
      // while we are in the midst of a sign-in flow.
      if (err.result == Cr.NS_ERROR_NOT_INITIALIZED) {
        log("Cannot sendAsyncMessage because the recipient frame has closed");
        return;
      }
      log("ERROR: sendAsyncMessage: " + err);
    }
  }
};

function IDPProvisioningContext(aID, aOrigin, aTargetMM) {
  this._id = aID;
  this._origin = aOrigin;
  this._mm = aTargetMM;
}

IDPProvisioningContext.prototype = {
  get id() { return this._id; },
  get origin() { return this._origin; },

  sendAsyncMessage: _sendAsyncMessage,

  doBeginProvisioningCallback: function IDPPC_doBeginProvCB(aID, aCertDuration) {
    let message = new IDDOMMessage({id: this.id});
    message.identity = aID;
    message.certDuration = aCertDuration;
    this.sendAsyncMessage("Identity:IDP:CallBeginProvisioningCallback",
                          message);
  },

  doGenKeyPairCallback: function IDPPC_doGenKeyPairCallback(aPublicKey) {
    log("doGenKeyPairCallback");
    let message = new IDDOMMessage({id: this.id});
    message.publicKey = aPublicKey;
    this.sendAsyncMessage("Identity:IDP:CallGenKeyPairCallback", message);
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
  get id() { return this._id; },
  get origin() { return this._origin; },

  sendAsyncMessage: _sendAsyncMessage,

  doBeginAuthenticationCallback: function IDPAC_doBeginAuthCB(aIdentity) {
    let message = new IDDOMMessage({id: this.id});
    message.identity = aIdentity;
    this.sendAsyncMessage("Identity:IDP:CallBeginAuthenticationCallback",
                          message);
  },

  doError: function IDPAC_doError(msg) {
    log("Authentication ERROR: " + msg);
  }
};

function RPWatchContext(aOptions, aTargetMM, aPrincipal) {
  objectCopy(aOptions, this);

  // id and origin are required
  if (! (this.id && this.origin)) {
    throw new Error("id and origin are required for RP watch context");
  }

  this.principal = aPrincipal;

  // default for no loggedInUser is undefined, not null
  this.loggedInUser = aOptions.loggedInUser;

  // Maybe internal.  For hosted b2g identity shim.
  this._internal = aOptions._internal;

  this._mm = aTargetMM;
}

RPWatchContext.prototype = {
  sendAsyncMessage: _sendAsyncMessage,

  doLogin: function RPWatchContext_onlogin(aAssertion, aMaybeInternalParams) {
    log("doLogin: " + this.id);
    let message = new IDDOMMessage({id: this.id, assertion: aAssertion});
    if (aMaybeInternalParams) {
      message._internalParams = aMaybeInternalParams;
    }
    this.sendAsyncMessage("Identity:RP:Watch:OnLogin", message);
  },

  doLogout: function RPWatchContext_onlogout() {
    log("doLogout: " + this.id);
    let message = new IDDOMMessage({id: this.id});
    this.sendAsyncMessage("Identity:RP:Watch:OnLogout", message);
  },

  doReady: function RPWatchContext_onready() {
    log("doReady: " + this.id);
    let message = new IDDOMMessage({id: this.id});
    this.sendAsyncMessage("Identity:RP:Watch:OnReady", message);
  },

  doCancel: function RPWatchContext_oncancel() {
    log("doCancel: " + this.id);
    let message = new IDDOMMessage({id: this.id});
    this.sendAsyncMessage("Identity:RP:Watch:OnCancel", message);
  },

  doError: function RPWatchContext_onerror(aMessage) {
    log("doError: " + this.id + ": " + JSON.stringify(aMessage));
    let message = new IDDOMMessage({id: this.id, message: aMessage});
    this.sendAsyncMessage("Identity:RP:Watch:OnError", message);
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
   * Mockable, for testing
   */
  _mockIdentityService: null,
  get IdentityService() {
    if (this._mockIdentityService) {
      log("Using a mocked identity service");
      return this._mockIdentityService;
    }
    return IdentityService;
  },

  /*
   * Create a new RPWatchContext, and update the context maps.
   */
  newContext: function(message, targetMM, principal) {
    let context = new RPWatchContext(message, targetMM, principal);
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
      log("ERROR: getService called before newContext for " + message.id);
      return null;
    }

    let context = this._serviceContexts.get(message.id);
    if (context.wantIssuer == "firefox-accounts") {
      if (Services.prefs.getPrefType(PREF_FXA_ENABLED) === Ci.nsIPrefBranch.PREF_BOOL
          && Services.prefs.getBoolPref(PREF_FXA_ENABLED)) {
        return FirefoxAccounts;
      }
      log("WARNING: Firefox Accounts is not enabled; Defaulting to BrowserID");
    }
    return this.IdentityService;
  },

  /*
   * Get the RPWatchContext object for a given message manager.
   */
  getContextForMM: function(targetMM) {
    return this._serviceContexts.get(this._mmContexts.get(targetMM));
  },

  hasContextForMM: function(targetMM) {
    return this._mmContexts.has(targetMM);
  },

  /*
   * Delete the RPWatchContext object for a given message manager.  Removes the
   * mapping both from _serviceContexts and _mmContexts.
   */
  deleteContextForMM: function(targetMM) {
    this._serviceContexts.delete(this._mmContexts.get(targetMM));
    this._mmContexts.delete(targetMM);
  },

  hasPermission: function(aMessage) {
    // We only check that the firefox accounts permission is present in the
    // manifest.
    if (aMessage.json && aMessage.json.wantIssuer == "firefox-accounts") {
      if (!aMessage.principal) {
        return false;
      }

      let permission =
        permissionManager.testPermissionFromPrincipal(aMessage.principal,
                                                      FXA_PERMISSION);
      return permission != Ci.nsIPermissionManager.UNKNOWN_ACTION &&
             permission != Ci.nsIPermissionManager.DENY_ACTION;
    }
    return true;
  },

  // nsIMessageListener
  receiveMessage: function DOMIdentity_receiveMessage(aMessage) {
    let msg = aMessage.json;

    // Target is the frame message manager that called us and is
    // used to send replies back to the proper window.
    let targetMM = aMessage.target;

    if (!this.hasPermission(aMessage)) {
      throw new Error("PERMISSION_DENIED");
    }

    switch (aMessage.name) {
      // RP
      case "Identity:RP:Watch":
        this._watch(msg, targetMM, aMessage.principal);
        break;
      case "Identity:RP:Unwatch":
        this._unwatch(msg, targetMM);
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
    if (!ppmm) {
      return;
    }
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

  _watch: function DOMIdentity__watch(message, targetMM, principal) {
    log("DOMIdentity__watch: " + message.id + " - " + principal);
    let context = this.newContext(message, targetMM, principal);
    this.getService(message).RP.watch(context);
  },

  _unwatch: function DOMIdentity_unwatch(message, targetMM) {
    log("DOMIDentity__unwatch: " + message.id);
    // If watch failed for some reason (e.g., exception thrown because RP did
    // not have the right callbacks, we don't want unwatch to throw, because it
    // will break the process of releasing the page's resources and leak
    // memory.
    let service = this.getService(message);
    if (service && service.RP) {
      service.RP.unwatch(message.id, targetMM);
      this.deleteContextForMM(targetMM);
      return;
    }
    log("Can't find a service to unwatch() for " + message.id);
  },

  _request: function DOMIdentity__request(message) {
    let service = this.getService(message);
    if (service && service.RP) {
      service.RP.request(message.id, message);
      return;
    }
    log("No context in which to call request(); Did you call watch() first?");
  },

  _logout: function DOMIdentity__logout(message) {
    let service = this.getService(message);
    if (service && service.RP) {
      service.RP.logout(message.id, message.origin, message);
      return;
    }
    log("No context in which to call logout(); Did you call watch() first?");
  },

  _childProcessShutdown: function DOMIdentity__childProcessShutdown(targetMM) {
    if (!this.hasContextForMM(targetMM)) {
      return;
    }

    let service = this.getContextForMM(targetMM);
    if (service && service.RP) {
      service.RP.childProcessShutdown(targetMM);
    }

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
