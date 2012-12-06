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
 * gaia context that is launched by the system application, with the
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
 * Controller in this module, which in turn triggers gaia to open a
 * window to host the Persona js.  If user interaction is required,
 * gaia will open the trusty UI.  If user interaction is not required,
 * and we only need to get to Persona functions, gaia will open a
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
Cu.import("resource://gre/modules/identity/IdentityUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "IdentityService",
                                  "resource://gre/modules/identity/MinimalIdentity.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Logger",
                                  "resource://gre/modules/identity/LogUtils.jsm");

// JS shim that contains the callback functions that
// live within the identity UI provisioning frame.
const kIdentityShimFile = "chrome://browser/content/identity.js";

// Type of MozChromeEvents to handle id dialogs.
const kOpenIdentityDialog = "open-id-dialog";
const kCloseIdentityDialog = "close-id-dialog";

// Observer messages to communicate to shim
const kReceivedIdentityAssertion = "received-id-assertion";
const kIdentityDelegateWatch = "identity-delegate-watch";
const kIdentityDelegateRequest = "identity-delegate-request";
const kIdentityDelegateLogout = "identity-delegate-logout";
const kIdentityDelegateFinished = "identity-delegate-finished";
const kIdentityDelegateReady = "identity-delegate-ready";

const kIdentityControllerDoMethod = "identity-controller-doMethod";

function log(...aMessageArgs) {
  Logger.log.apply(Logger, ["SignInToWebsiteController"].concat(aMessageArgs));
}

/*
 * GaiaInterface encapsulates the our gaia functions.  There are only two:
 *
 * getContent       - return the current content window
 * sendChromeEvent  - send a chromeEvent from the browser shell
 */
let GaiaInterface = {
  _getBrowser: function SignInToWebsiteController__getBrowser() {
    return Services.wm.getMostRecentWindow("navigator:browser");
  },

  getContent: function SignInToWebsiteController_getContent() {
    return this._getBrowser().getContentWindow();
  },

  sendChromeEvent: function SignInToWebsiteController_sendChromeEvent(detail) {
    this._getBrowser().shell.sendChromeEvent(detail);
  }
};

/*
 * The Pipe abstracts the communcation channel between the Controller
 * and the identity.js code running in the browser window.
 */
let Pipe = {

  /*
   * communicate - launch a gaia window with certain options and
   * provide a callback for handling messages.
   *
   * @param aRpOptions        options describing the Relying Party's
   *        (dictionary)      call, such as origin and loggedInUser.
   *
   * @param aGaiaOptions      showUI:   boolean
   *        (dictionary)      message:  name of the message to emit
   *                                    (request, logout, watch)
   *
   * @param aMessageCallback  function to call on receipt of a
   *        (function)        do-method message.  These messages name
   *                          a method ('login', 'logout', etc.) and
   *                          carry optional parameters.  The Pipe does
   *                          not know what the messages mean; it is
   *                          up to the caller to interpret them and
   *                          act on them.
   */
  communicate: function(aRpOptions, aGaiaOptions, aMessageCallback) {
    log("open gaia dialog with options:", aGaiaOptions);

    // This content variable is injected into the scope of
    // kIdentityShimFile, where it is used to access the BrowserID object
    // and its internal API.
    let content = GaiaInterface.getContent();

    if (!content) {
      log("ERROR: what the what? no content window?");
      // aErrorCb.onresult("NO_CONTENT_WINDOW");
      return;
    }

    // Prepare a message for gaia.  The parameter showUI signals
    // whether user interaction is needed.  If it is, gaia will open a
    // dialog; if not, a hidden iframe.  In each case, BrowserID is
    // available in the context.
    let id = kOpenIdentityDialog + "-" + getRandomId();
    let detail = {
      type: kOpenIdentityDialog,
      showUI: aGaiaOptions.showUI || false,
      id: id
    };

    // When gaia signals back with a mozContentEvent containing the
    // unique id we created, we know the window is ready.  We then inject
    // the magic javascript (kIdentityShimFile) that will give the content
    // the superpowers it needs to communicate back with this code.
    content.addEventListener("mozContentEvent", function getAssertion(evt) {

      // Make sure the message is really for us
      let msg = evt.detail;
      if (msg.id != id) {
        return;
      }

      // We only need to catch the first mozContentEvent from the
      // iframe or popup, so we remove the listener right away.
      content.removeEventListener("mozContentEvent", getAssertion);

      // Try to load the identity shim file containing the callbacks
      // in the content script.  This could be either the visible
      // popup that the user interacts with, or it could be an invisible
      // frame.
      let frame = evt.detail.frame;
      let frameLoader = frame.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader;
      let mm = frameLoader.messageManager;
      try {
        mm.loadFrameScript(kIdentityShimFile, true);
        log("Loaded shim " + kIdentityShimFile + "\n");
      } catch (e) {
        log("Error loading ", kIdentityShimFile, " as a frame script: ", e);
      }

      // There are two messages that the delegate can send back: a "do
      // method" event, and a "finished" event.  We pass the do-method
      // events straight to the caller for interpretation and handling.
      // If we receive a "finished" event, then the delegate is done, so
      // we shut down the pipe and clean up.
      mm.addMessageListener(kIdentityControllerDoMethod, aMessageCallback);
      mm.addMessageListener(kIdentityDelegateFinished, function identityDelegateFinished() {
        // clean up listeners
        mm.removeMessageListener(kIdentityDelegateFinished, identityDelegateFinished);
        mm.removeMessageListener(kIdentityControllerDoMethod, aMessageCallback);

        let id = kReceivedIdentityAssertion + "-" + getRandomId();
        let detail = {
          type: kReceivedIdentityAssertion,
          showUI: aGaiaOptions.showUI || false,
          id: id
        };
        log('telling gaia to close the dialog');
        // tell gaia to close the dialog
        GaiaInterface.sendChromeEvent(detail);
      });

      mm.sendAsyncMessage(aGaiaOptions.message, aRpOptions);
    });

    // Tell gaia to open the identity iframe or trusty popup
    GaiaInterface.sendChromeEvent(detail);
  }

};

/*
 * The controller sits between the IdentityService used by DOMIdentity
 * and a gaia process launches an (invisible) iframe or (visible)
 * trusty UI.  Using an injected js script (identity.js), the
 * controller enables the gaia window to access the persona identity
 * storage in the system cookie jar and send events back via the
 * controller into IdentityService and DOM, and ultimately up to the
 * Relying Party, which is open in a different window context.
 */
this.SignInToWebsiteController = {

  /*
   * Initialize the controller.  To use a different gaia communication pipe,
   * such as when mocking it in tests, pass aOptions.pipe.
   */
  init: function SignInToWebsiteController_init(aOptions) {
    aOptions = aOptions || {};
    this.pipe = aOptions.pipe || Pipe;
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
    switch(aTopic) {
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

      switch(message.method) {
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

        default:
          log("WARNING: wonky method call:", message.method);
          break;
      }
    };
  },

  doWatch: function SignInToWebsiteController_doWatch(aRpOptions) {
    // dom prevents watch from  being called twice
    var gaiaOptions = {
      message: kIdentityDelegateWatch,
      showUI: false
    };
    this.pipe.communicate(aRpOptions, gaiaOptions, this._makeDoMethodCallback(aRpOptions.id));
  },

  /**
   * The website is requesting login so the user must choose an identity to use.
   */
  doRequest: function SignInToWebsiteController_doRequest(aRpOptions) {
    log("doRequest", aRpOptions);
    // tell gaia to open the identity popup
    var gaiaOptions = {
      message: kIdentityDelegateRequest,
      showUI: true
    };
    this.pipe.communicate(aRpOptions, gaiaOptions, this._makeDoMethodCallback(aRpOptions.id));
  },

  /*
   *
   */
  doLogout: function SignInToWebsiteController_doLogout(aRpOptions) {
    log("doLogout", aRpOptions);
    var gaiaOptions = {
      message: kIdentityDelegateLogout,
      showUI: false
    };
    this.pipe.communicate(aRpOptions, gaiaOptions, this._makeDoMethodCallback(aRpOptions.id));
  }

};
