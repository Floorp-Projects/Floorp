/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * SignInToWebsite.jsm - UX Controller and means for accessing identity
 * cookies on behalf of relying parties.
 *
 * Currently, the b2g security architecture isolates web applications
 * so that each window has access only to a local cookie jar:
 *
 *     To prevent Web apps from interfering with one another, each one is
 *     hosted on a separate domain, and therefore may only access the
 *     resources associated with its domain. These resources include
 *     things such as IndexedDB databases, cookies, offline storage,
 *     and so forth.
 *
 *     -- https://developer.mozilla.org/en-US/docs/Mozilla/Firefox_OS/Security/Security_model
 *
 * As a result, an authentication system like Persona cannot share its
 * cookie jar with multiple relying parties, and so would require a
 * fresh login request in every window.  This would not be a good
 * experience.
 *
 *
 * In order for navigator.id.request() to maintain state in a single
 * cookie jar, we cause all Persona interactions to take place in a
 * content context that is launched by the system application, with the
 * result that Persona has a single cookie jar that all Relying
 * Parties can use.  Since of course those Relying Parties cannot
 * reach into the system cookie jar, the Controller in this module
 * provides a way to get messages and data to and fro between the
 * Relying Party in its window context, and the Persona internal api
 * in its context.
 *
 * On the Relying Party's side, say a web page invokes
 * navigator.id.watch(), to register callbacks, and then
 * navigator.id.request() to request an assertion.  The navigator.id
 * calls are provided by nsDOMIdentity.  nsDOMIdentity messages down
 * to the privileged DOMIdentity code (using cpmm and ppmm message
 * managers).  DOMIdentity stores the state of Relying Party flows
 * using an Identity service (MinimalIdentity.jsm), and emits messages
 * requesting Persona functions (doWatch, doReady, doLogout).
 *
 * The Identity service sends these observer messages to the
 * Controller in this module, which in turn triggers content to open a
 * window to host the Persona js.  If user interaction is required,
 * content will open the trusty UI.  If user interaction is not required,
 * and we only need to get to Persona functions, content will open a
 * hidden iframe.  In either case, a window is opened into which the
 * controller causes the script identity.js to be injected.  This
 * script provides the glue between the in-page javascript and the
 * pipe back down to the Controller, translating navigator.internal
 * function callbacks into messages sent back to the Controller.
 *
 * As a result, a navigator.internal function in the hosted popup or
 * iframe can call back to the injected identity.js (doReady, doLogin,
 * or doLogout).  identity.js callbacks send messages back through the
 * pipe to the Controller.  The controller invokes the corresponding
 * function on the Identity Service (doReady, doLogin, or doLogout).
 * The IdentityService calls the corresponding callback for the
 * correct Relying Party, which causes DOMIdentity to send a message
 * up to the Relying Party through nsDOMIdentity
 * (Identity:RP:Watch:OnLogin etc.), and finally, nsDOMIdentity
 * receives these messages and calls the original callback that the
 * Relying Party registered (navigator.id.watch(),
 * navigator.id.request(), or navigator.id.logout()).
 */

"use strict";

this.EXPORTED_SYMBOLS = ["SignInToWebsiteController"];

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "getRandomId",
                                  "resource://gre/modules/identity/IdentityUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "IdentityService",
                                  "resource://gre/modules/identity/MinimalIdentity.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Logger",
                                  "resource://gre/modules/identity/LogUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SystemAppProxy",
                                  "resource://gre/modules/SystemAppProxy.jsm");

// The default persona uri; can be overwritten with toolkit.identity.uri pref.
// Do this if you want to repoint to a different service for testing.
// There's no point in setting up an observer to monitor the pref, as b2g prefs
// can only be overwritten when the profie is recreated.  So just get the value
// on start-up.
var kPersonaUri = "https://firefoxos.persona.org";
try {
  kPersonaUri = Services.prefs.getCharPref("toolkit.identity.uri");
} catch(noSuchPref) {
  // stick with the default value
}

// JS shim that contains the callback functions that
// live within the identity UI provisioning frame.
const kIdentityShimFile = "chrome://b2g/content/identity.js";

// Type of MozChromeEvents to handle id dialogs.
const kOpenIdentityDialog = "id-dialog-open";
const kDoneIdentityDialog = "id-dialog-done";
const kCloseIdentityDialog = "id-dialog-close-iframe";

// Observer messages to communicate to shim
const kIdentityDelegateWatch = "identity-delegate-watch";
const kIdentityDelegateRequest = "identity-delegate-request";
const kIdentityDelegateLogout = "identity-delegate-logout";
const kIdentityDelegateFinished = "identity-delegate-finished";
const kIdentityDelegateReady = "identity-delegate-ready";

const kIdentityControllerDoMethod = "identity-controller-doMethod";

function log(...aMessageArgs) {
  Logger.log.apply(Logger, ["SignInToWebsiteController"].concat(aMessageArgs));
}

log("persona uri =", kPersonaUri);

function sendChromeEvent(details) {
  details.uri = kPersonaUri;
  SystemAppProxy.dispatchEvent(details);
}

function Pipe() {
  this._watchers = [];
}

Pipe.prototype = {
  init: function pipe_init() {
    Services.obs.addObserver(this, "identity-child-process-shutdown", false);
    Services.obs.addObserver(this, "identity-controller-unwatch", false);
  },

  uninit: function pipe_uninit() {
    Services.obs.removeObserver(this, "identity-child-process-shutdown");
    Services.obs.removeObserver(this, "identity-controller-unwatch");
  },

  observe: function Pipe_observe(aSubject, aTopic, aData) {
    let options = {};
    if (aSubject) {
      options = aSubject.wrappedJSObject;
    }
    switch (aTopic) {
      case "identity-child-process-shutdown":
        log("pipe removing watchers by message manager");
        this._removeWatchers(null, options.messageManager);
        break;

      case "identity-controller-unwatch":
        log("unwatching", options.id);
        this._removeWatchers(options.id, options.messageManager);
        break;
    }
  },

  _addWatcher: function Pipe__addWatcher(aId, aMm) {
    log("Adding watcher with id", aId);
    for (let i = 0; i < this._watchers.length; ++i) {
      let watcher = this._watchers[i];
      if (this._watcher.id === aId) {
        watcher.count++;
        return;
      }
    }
    this._watchers.push({id: aId, count: 1, mm: aMm});
  },

  _removeWatchers: function Pipe__removeWatcher(aId, aMm) {
    let checkId = aId !== null;
    let index = -1;
    for (let i = 0; i < this._watchers.length; ++i) {
      let watcher = this._watchers[i];
      if (watcher.mm === aMm &&
          (!checkId || (checkId && watcher.id === aId))) {
        index = i;
        break;
      }
    }

    if (index !== -1) {
      if (checkId) {
        if (--(this._watchers[index].count) === 0) {
          this._watchers.splice(index, 1);
        }
      } else {
        this._watchers.splice(index, 1);
      }
    }

    if (this._watchers.length === 0) {
      log("No more watchers; clean up persona host iframe");
      let detail = {
        type: kCloseIdentityDialog
      };
      log('telling content to close the dialog');
      // tell content to close the dialog
      sendChromeEvent(detail);
    }
  },

  communicate: function(aRpOptions, aContentOptions, aMessageCallback) {
    let rpID = aRpOptions.id;
    let rpMM = aRpOptions.mm;
    if (rpMM) {
      this._addWatcher(rpID, rpMM);
    }

    log("RP options:", aRpOptions, "\n  content options:", aContentOptions);

    // This content variable is injected into the scope of
    // kIdentityShimFile, where it is used to access the BrowserID object
    // and its internal API.
    let mm = null;
    let uuid = getRandomId();
    let self = this;

    function removeMessageListeners() {
      if (mm) {
        mm.removeMessageListener(kIdentityDelegateFinished, identityDelegateFinished);
        mm.removeMessageListener(kIdentityControllerDoMethod, aMessageCallback);
      }
    }

    function identityDelegateFinished() {
      removeMessageListeners();

      let detail = {
        type: kDoneIdentityDialog,
        showUI: aContentOptions.showUI || false,
        id: kDoneIdentityDialog + "-" + uuid,
        requestId: aRpOptions.id
      };
      log('received delegate finished; telling content to close the dialog');
      sendChromeEvent(detail);
      self._removeWatchers(rpID, rpMM);
    }

    SystemAppProxy.addEventListener("mozContentEvent", function getAssertion(evt) {
      let msg = evt.detail;
      if (!msg.id.match(uuid)) {
        return;
      }

      switch (msg.id) {
        case kOpenIdentityDialog + '-' + uuid:
          if (msg.type === 'cancel') {
            // The user closed the dialog.  Clean up and call cancel.
            SystemAppProxy.removeEventListener("mozContentEvent", getAssertion);
            removeMessageListeners();
            aMessageCallback({json: {method: "cancel"}});
          } else {
            // The window has opened.  Inject the identity shim file containing
            // the callbacks in the content script.  This could be either the
            // visible popup that the user interacts with, or it could be an
            // invisible frame.
            let frame = evt.detail.frame;
            let frameLoader = frame.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader;
            mm = frameLoader.messageManager;
            try {
              mm.loadFrameScript(kIdentityShimFile, true, true);
              log("Loaded shim", kIdentityShimFile);
            } catch (e) {
              log("Error loading", kIdentityShimFile, "as a frame script:", e);
            }

            // There are two messages that the delegate can send back: a "do
            // method" event, and a "finished" event.  We pass the do-method
            // events straight to the caller for interpretation and handling.
            // If we receive a "finished" event, then the delegate is done, so
            // we shut down the pipe and clean up.
            mm.addMessageListener(kIdentityControllerDoMethod, aMessageCallback);
            mm.addMessageListener(kIdentityDelegateFinished, identityDelegateFinished);

            mm.sendAsyncMessage(aContentOptions.message, aRpOptions);
          }
          break;

        case kDoneIdentityDialog + '-' + uuid:
          // Received our assertion.  The message manager callbacks will handle
          // communicating back to the IDService.  All we have to do is remove
          // this listener.
          SystemAppProxy.removeEventListener("mozContentEvent", getAssertion);
          break;

        default:
          log("ERROR - Unexpected message: id=" + msg.id + ", type=" + msg.type + ", errorMsg=" + msg.errorMsg);
          break;
      }

    });

    // Tell content to open the identity iframe or trusty popup. The parameter
    // showUI signals whether user interaction is needed.  If it is, content will
    // open a dialog; if not, a hidden iframe.  In each case, BrowserID is
    // available in the context.
    let detail = {
      type: kOpenIdentityDialog,
      showUI: aContentOptions.showUI || false,
      id: kOpenIdentityDialog + "-" + uuid,
      requestId: aRpOptions.id
    };

    sendChromeEvent(detail);
  }

};

/*
 * The controller sits between the IdentityService used by DOMIdentity
 * and a content process launches an (invisible) iframe or (visible)
 * trusty UI.  Using an injected js script (identity.js), the
 * controller enables the content window to access the persona identity
 * storage in the system cookie jar and send events back via the
 * controller into IdentityService and DOM, and ultimately up to the
 * Relying Party, which is open in a different window context.
 */
this.SignInToWebsiteController = {

  /*
   * Initialize the controller.  To use a different content communication pipe,
   * such as when mocking it in tests, pass aOptions.pipe.
   */
  init: function SignInToWebsiteController_init(aOptions) {
    aOptions = aOptions || {};
    this.pipe = aOptions.pipe || new Pipe();
    Services.obs.addObserver(this, "identity-controller-watch", false);
    Services.obs.addObserver(this, "identity-controller-request", false);
    Services.obs.addObserver(this, "identity-controller-logout", false);
  },

  uninit: function SignInToWebsiteController_uninit() {
    Services.obs.removeObserver(this, "identity-controller-watch");
    Services.obs.removeObserver(this, "identity-controller-request");
    Services.obs.removeObserver(this, "identity-controller-logout");
  },

  observe: function SignInToWebsiteController_observe(aSubject, aTopic, aData) {
    log("observe: received", aTopic, "with", aData, "for", aSubject);
    let options = null;
    if (aSubject) {
      options = aSubject.wrappedJSObject;
    }
    switch (aTopic) {
      case "identity-controller-watch":
        this.doWatch(options);
        break;
      case "identity-controller-request":
        this.doRequest(options);
        break;
      case "identity-controller-logout":
        this.doLogout(options);
        break;
      default:
        Logger.reportError("SignInToWebsiteController", "Unknown observer notification:", aTopic);
        break;
    }
  },

  /*
   * options:    method          required - name of method to invoke
   *             assertion       optional
   */
  _makeDoMethodCallback: function SignInToWebsiteController__makeDoMethodCallback(aRpId) {
    return function SignInToWebsiteController_methodCallback(aOptions) {
      let message = aOptions.json;
      if (typeof message === 'string') {
        message = JSON.parse(message);
      }

      switch (message.method) {
        case "ready":
          IdentityService.doReady(aRpId);
          break;

        case "login":
           if (message._internalParams) {
             IdentityService.doLogin(aRpId, message.assertion, message._internalParams);
           } else {
             IdentityService.doLogin(aRpId, message.assertion);
           }
          break;

        case "logout":
          IdentityService.doLogout(aRpId);
          break;

        case "cancel":
          IdentityService.doCancel(aRpId);
          break;

        default:
          log("WARNING: wonky method call:", message.method);
          break;
      }
    };
  },

  doWatch: function SignInToWebsiteController_doWatch(aRpOptions) {
    // dom prevents watch from  being called twice
    let contentOptions = {
      message: kIdentityDelegateWatch,
      showUI: false
    };
    this.pipe.communicate(aRpOptions, contentOptions,
        this._makeDoMethodCallback(aRpOptions.id));
  },

  /**
   * The website is requesting login so the user must choose an identity to use.
   */
  doRequest: function SignInToWebsiteController_doRequest(aRpOptions) {
    log("doRequest", aRpOptions);
    let contentOptions = {
      message: kIdentityDelegateRequest,
      showUI: true
    };
    this.pipe.communicate(aRpOptions, contentOptions,
        this._makeDoMethodCallback(aRpOptions.id));
  },

  /*
   *
   */
  doLogout: function SignInToWebsiteController_doLogout(aRpOptions) {
    log("doLogout", aRpOptions);
    let contentOptions = {
      message: kIdentityDelegateLogout,
      showUI: false
    };
    this.pipe.communicate(aRpOptions, contentOptions,
        this._makeDoMethodCallback(aRpOptions.id));
  }

};
