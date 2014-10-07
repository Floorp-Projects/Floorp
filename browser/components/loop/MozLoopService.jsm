/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

// Invalid auth token as per
// https://github.com/mozilla-services/loop-server/blob/45787d34108e2f0d87d74d4ddf4ff0dbab23501c/loop/errno.json#L6
const INVALID_AUTH_TOKEN = 110;

// Ticket numbers are 24 bits in length.
// The highest valid ticket number is 16777214 (2^24 - 2), so that a "now
// serving" number of 2^24 - 1 is greater than it.
const MAX_SOFT_START_TICKET_NUMBER = 16777214;

const LOOP_SESSION_TYPE = {
  GUEST: 1,
  FXA: 2,
};

// See LOG_LEVELS in Console.jsm. Common examples: "All", "Info", "Warn", & "Error".
const PREF_LOG_LEVEL = "loop.debug.loglevel";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/osfile.jsm", this);
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/FxAccountsOAuthClient.jsm");
Cu.importGlobalProperties(["URL"]);

this.EXPORTED_SYMBOLS = ["MozLoopService", "LOOP_SESSION_TYPE"];

XPCOMUtils.defineLazyModuleGetter(this, "injectLoopAPI",
  "resource:///modules/loop/MozLoopAPI.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "convertToRTCStatsReport",
  "resource://gre/modules/media/RTCStatsReport.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Chat", "resource:///modules/Chat.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "CommonUtils",
                                  "resource://services-common/utils.js");

XPCOMUtils.defineLazyModuleGetter(this, "CryptoUtils",
                                  "resource://services-crypto/utils.js");

XPCOMUtils.defineLazyModuleGetter(this, "FxAccountsProfileClient",
                                  "resource://gre/modules/FxAccountsProfileClient.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "HawkClient",
                                  "resource://services-common/hawkclient.js");

XPCOMUtils.defineLazyModuleGetter(this, "deriveHawkCredentials",
                                  "resource://services-common/hawkrequest.js");

XPCOMUtils.defineLazyModuleGetter(this, "MozLoopPushHandler",
                                  "resource:///modules/loop/MozLoopPushHandler.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "uuidgen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

XPCOMUtils.defineLazyServiceGetter(this, "gDNSService",
                                   "@mozilla.org/network/dns-service;1",
                                   "nsIDNSService");

// Create a new instance of the ConsoleAPI so we can control the maxLogLevel with a pref.
XPCOMUtils.defineLazyGetter(this, "log", () => {
  let ConsoleAPI = Cu.import("resource://gre/modules/devtools/Console.jsm", {}).ConsoleAPI;
  let consoleOptions = {
    maxLogLevel: Services.prefs.getCharPref(PREF_LOG_LEVEL).toLowerCase(),
    prefix: "Loop",
  };
  return new ConsoleAPI(consoleOptions);
});

// The current deferred for the registration process. This is set if in progress
// or the registration was successful. This is null if a registration attempt was
// unsuccessful.
let gRegisteredDeferred = null;
let gPushHandler = null;
let gHawkClient = null;
let gLocalizedStrings =  null;
let gInitializeTimer = null;
let gFxAEnabled = true;
let gFxAOAuthClientPromise = null;
let gFxAOAuthClient = null;
let gFxAOAuthTokenData = null;
let gFxAOAuthProfile = null;
let gErrors = new Map();

 /**
 * Attempts to open a websocket.
 *
 * A new websocket interface is used each time. If an onStop callback
 * was received, calling asyncOpen() on the same interface will
 * trigger a "alreay open socket" exception even though the channel
 * is logically closed.
 */
function CallProgressSocket(progressUrl, callId, token) {
  if (!progressUrl || !callId || !token) {
    throw new Error("missing required arguments");
  }

  this._progressUrl = progressUrl;
  this._callId = callId;
  this._token = token;
}

CallProgressSocket.prototype = {
  /**
   * Open websocket and run hello exchange.
   * Sends a hello message to the server.
   *
   * @param {function} Callback used after a successful handshake
   *                   over the progressUrl.
   * @param {function} Callback used if an error is encountered
   */
  connect: function(onSuccess, onError) {
    this._onSuccess = onSuccess;
    this._onError = onError ||
      (reason => {log.warn("MozLoopService::callProgessSocket - ", reason);});

    if (!onSuccess) {
      this._onError("missing onSuccess argument");
      return;
    }

    if (Services.io.offline) {
      this._onError("IO offline");
      return;
    }

    let uri = Services.io.newURI(this._progressUrl, null, null);

    // Allow _websocket to be set for testing.
    this._websocket = this._websocket ||
      Cc["@mozilla.org/network/protocol;1?name=" + uri.scheme]
        .createInstance(Ci.nsIWebSocketChannel);

    this._websocket.asyncOpen(uri, this._progressUrl, this, null);
  },

  /**
   * Listener method, handles the start of the websocket stream.
   * Sends a hello message to the server.
   *
   * @param {nsISupports} aContext Not used
   */
  onStart: function() {
    let helloMsg = {
      messageType: "hello",
      callId: this._callId,
      auth: this._token,
    };
    try { // in case websocket has closed before this handler is run
      this._websocket.sendMsg(JSON.stringify(helloMsg));
    }
    catch (error) {
      this._onError(error);
    }
  },

  /**
   * Listener method, called when the websocket is closed.
   *
   * @param {nsISupports} aContext Not used
   * @param {nsresult} aStatusCode Reason for stopping (NS_OK = successful)
   */
  onStop: function(aContext, aStatusCode) {
    if (!this._handshakeComplete) {
      this._onError("[" + aStatusCode + "]");
    }
  },

  /**
   * Listener method, called when the websocket is closed by the server.
   * If there are errors, onStop may be called without ever calling this
   * method.
   *
   * @param {nsISupports} aContext Not used
   * @param {integer} aCode the websocket closing handshake close code
   * @param {String} aReason the websocket closing handshake close reason
   */
  onServerClose: function(aContext, aCode, aReason) {
    if (!this._handshakeComplete) {
      this._onError("[" + aCode + "]" + aReason);
    }
  },

  /**
   * Listener method, called when the websocket receives a message.
   *
   * @param {nsISupports} aContext Not used
   * @param {String} aMsg The message data
   */
  onMessageAvailable: function(aContext, aMsg) {
    let msg = {};
    try {
      msg = JSON.parse(aMsg);
    }
    catch (error) {
      log.error("MozLoopService: error parsing progress message - ", error);
      return;
    }

    if (msg.messageType && msg.messageType === 'hello') {
      this._handshakeComplete = true;
      this._onSuccess();
    }
  },


  /**
   * Create a JSON message payload and send on websocket.
   *
   * @param {Object} aMsg Message to send.
   */
  _send: function(aMsg) {
    if (!this._handshakeComplete) {
      log.warn("MozLoopService::_send error - handshake not complete");
      return;
    }

    try {
      this._websocket.sendMsg(JSON.stringify(aMsg));
    }
    catch (error) {
      this._onError(error);
    }
  },

  /**
   * Notifies the server that the user has declined the call
   * with a reason of busy.
   */
  sendBusy: function() {
    this._send({
      messageType: "action",
      event: "terminate",
      reason: "busy"
    });
  },
};

/**
 * Internal helper methods and state
 *
 * The registration is a two-part process. First we need to connect to
 * and register with the push server. Then we need to take the result of that
 * and register with the Loop server.
 */
let MozLoopServiceInternal = {
  callsData: {inUse: false},
  _mocks: {webSocket: undefined},

  // The uri of the Loop server.
  get loopServerUri() Services.prefs.getCharPref("loop.server"),

  /**
   * The initial delay for push registration. This ensures we don't start
   * kicking off straight after browser startup, just a few seconds later.
   */
  get initialRegistrationDelayMilliseconds() {
    try {
      // Let a pref override this for developer & testing use.
      return Services.prefs.getIntPref("loop.initialDelay");
    } catch (x) {
      // Default to 5 seconds
      return 5000;
    }
    return initialDelay;
  },

  /**
   * Gets the current latest expiry time for urls.
   *
   * In seconds since epoch.
   */
  get expiryTimeSeconds() {
    try {
      return Services.prefs.getIntPref("loop.urlsExpiryTimeSeconds");
    } catch (x) {
      // It is ok for the pref not to exist.
      return 0;
    }
  },

  /**
   * Sets the expiry time to either the specified time, or keeps it the same
   * depending on which is latest.
   */
  set expiryTimeSeconds(time) {
    if (time > this.expiryTimeSeconds) {
      Services.prefs.setIntPref("loop.urlsExpiryTimeSeconds", time);
    }
  },

  /**
   * Returns true if the expiry time is in the future.
   */
  urlExpiryTimeIsInFuture: function() {
    return this.expiryTimeSeconds * 1000 > Date.now();
  },

  /**
   * Retrieves MozLoopService "do not disturb" pref value.
   *
   * @return {Boolean} aFlag
   */
  get doNotDisturb() {
    return Services.prefs.getBoolPref("loop.do_not_disturb");
  },

  /**
   * Sets MozLoopService "do not disturb" pref value.
   *
   * @param {Boolean} aFlag
   */
  set doNotDisturb(aFlag) {
    Services.prefs.setBoolPref("loop.do_not_disturb", Boolean(aFlag));
    this.notifyStatusChanged();
  },

  notifyStatusChanged: function(aReason = null) {
    log.debug("notifyStatusChanged with reason:", aReason);
    Services.obs.notifyObservers(null, "loop-status-changed", aReason);
  },

  /**
   * Record an error and notify interested UI with the relevant user-facing strings attached.
   *
   * @param {String} errorType a key to identify the type of error. Only one
   *                           error of a type will be saved at a time. This value may be used to
   *                           determine user-facing (aka. friendly) strings.
   * @param {Object} error     an object describing the error in the format from Hawk errors
   */
  setError: function(errorType, error) {
    let messageString, detailsString, detailsButtonLabelString;
    const NETWORK_ERRORS = [
      Cr.NS_ERROR_CONNECTION_REFUSED,
      Cr.NS_ERROR_NET_INTERRUPT,
      Cr.NS_ERROR_NET_RESET,
      Cr.NS_ERROR_NET_TIMEOUT,
      Cr.NS_ERROR_OFFLINE,
      Cr.NS_ERROR_PROXY_CONNECTION_REFUSED,
      Cr.NS_ERROR_UNKNOWN_HOST,
      Cr.NS_ERROR_UNKNOWN_PROXY_HOST,
    ];

    if (error.code === null && error.errno === null &&
        error.error instanceof Ci.nsIException &&
        NETWORK_ERRORS.indexOf(error.error.result) != -1) {
      // Network error. Override errorType so we can easily clear it on the next succesful request.
      errorType = "network";
      messageString = "could_not_connect";
      detailsString = "check_internet_connection";
      detailsButtonLabelString = "retry_button";
    } else if (errorType == "profile" && error.code >= 500 && error.code < 600) {
      messageString = "problem_accessing_account";
    } else if (error.code == 401) {
      if (errorType == "login") {
        messageString = "could_not_authenticate"; // XXX: Bug 1076377
        detailsString = "password_changed_question";
        detailsButtonLabelString = "retry_button";
      } else {
        messageString = "session_expired_error_description";
      }
    } else if (error.code >= 500 && error.code < 600) {
      messageString = "service_not_available";
      detailsString = "try_again_later";
      detailsButtonLabelString = "retry_button";
    } else {
      messageString = "generic_failure_title";
    }

    error.friendlyMessage = this.localizedStrings[messageString].textContent;
    error.friendlyDetails = detailsString ?
                              this.localizedStrings[detailsString].textContent :
                              null;
    error.friendlyDetailsButtonLabel = detailsButtonLabelString ?
                                         this.localizedStrings[detailsButtonLabelString].textContent :
                                         null;

    gErrors.set(errorType, error);
    this.notifyStatusChanged();
  },

  clearError: function(errorType) {
    gErrors.delete(errorType);
    this.notifyStatusChanged();
  },

  get errors() {
    return gErrors;
  },

  /**
   * Starts registration of Loop with the push server, and then will register
   * with the Loop server. It will return early if already registered.
   *
   * @param {Object} mockPushHandler Optional, test-only mock push handler. Used
   *                                 to allow mocking of the MozLoopPushHandler.
   * @returns {Promise} a promise that is resolved with no params on completion, or
   *          rejected with an error code or string.
   */
  promiseRegisteredWithServers: function(mockPushHandler, mockWebSocket) {
    this._mocks.webSocket = mockWebSocket;

    if (gRegisteredDeferred) {
      return gRegisteredDeferred.promise;
    }

    gRegisteredDeferred = Promise.defer();
    // We grab the promise early in case .initialize or its results sets
    // it back to null on error.
    let result = gRegisteredDeferred.promise;

    gPushHandler = mockPushHandler || MozLoopPushHandler;

    gPushHandler.initialize(this.onPushRegistered.bind(this),
      this.onHandleNotification.bind(this));

    return result;
  },

  /**
   * Performs a hawk based request to the loop server.
   *
   * @param {LOOP_SESSION_TYPE} sessionType The type of session to use for the request.
   *                                        This is one of the LOOP_SESSION_TYPE members.
   * @param {String} path The path to make the request to.
   * @param {String} method The request method, e.g. 'POST', 'GET'.
   * @param {Object} payloadObj An object which is converted to JSON and
   *                            transmitted with the request.
   * @returns {Promise}
   *        Returns a promise that resolves to the response of the API call,
   *        or is rejected with an error.  If the server response can be parsed
   *        as JSON and contains an 'error' property, the promise will be
   *        rejected with this JSON-parsed response.
   */
  hawkRequest: function(sessionType, path, method, payloadObj) {
    if (!gHawkClient) {
      gHawkClient = new HawkClient(this.loopServerUri);
    }

    let sessionToken;
    try {
      sessionToken = Services.prefs.getCharPref(this.getSessionTokenPrefName(sessionType));
    } catch (x) {
      // It is ok for this not to exist, we'll default to sending no-creds
    }

    let credentials;
    if (sessionToken) {
      // true = use a hex key, as required by the server (see bug 1032738).
      credentials = deriveHawkCredentials(sessionToken, "sessionToken",
                                          2 * 32, true);
    }

    return gHawkClient.request(path, method, credentials, payloadObj).then((result) => {
      this.clearError("network");
      return result;
    }, (error) => {
      if (error.code == 401) {
        this.clearSessionToken(sessionType);

        if (sessionType == LOOP_SESSION_TYPE.FXA) {
          MozLoopService.logOutFromFxA().then(() => {
            // Set a user-visible error after logOutFromFxA clears existing ones.
            this.setError("login", error);
          });
        } else {
          if (!this.urlExpiryTimeIsInFuture()) {
            // If there are no Guest URLs in the future, don't use setError to notify the user since
            // there isn't a need for a Guest registration at this time.
            throw error;
          }

          this.setError("registration", error);
        }
      }
      throw error;
    });
  },

  /**
   * Generic hawkRequest onError handler for the hawkRequest promise.
   *
   * @param {Object} error - error reporting object
   *
   */

  _hawkRequestError: function(error) {
    log.error("Loop hawkRequest error:", error);
    throw error;
  },

  getSessionTokenPrefName: function(sessionType) {
    let suffix;
    switch (sessionType) {
      case LOOP_SESSION_TYPE.GUEST:
        suffix = "";
        break;
      case LOOP_SESSION_TYPE.FXA:
        suffix = ".fxa";
        break;
      default:
        throw new Error("Unknown LOOP_SESSION_TYPE");
        break;
    }
    return "loop.hawk-session-token" + suffix;
  },

  /**
   * Used to store a session token from a request if it exists in the headers.
   *
   * @param {LOOP_SESSION_TYPE} sessionType The type of session to use for the request.
   *                                        One of the LOOP_SESSION_TYPE members.
   * @param {Object} headers The request headers, which may include a
   *                         "hawk-session-token" to be saved.
   * @return true on success or no token, false on failure.
   */
  storeSessionToken: function(sessionType, headers) {
    let sessionToken = headers["hawk-session-token"];
    if (sessionToken) {
      // XXX should do more validation here
      if (sessionToken.length === 64) {
        Services.prefs.setCharPref(this.getSessionTokenPrefName(sessionType), sessionToken);
        log.debug("Stored a hawk session token for sessionType", sessionType);
      } else {
        // XXX Bubble the precise details up to the UI somehow (bug 1013248).
        log.warn("Loop server sent an invalid session token");
        gRegisteredDeferred.reject("session-token-wrong-size");
        gRegisteredDeferred = null;
        return false;
      }
    }
    return true;
  },


  /**
   * Clear the loop session token so we don't use it for Hawk Requests anymore.
   *
   * This should normally be used after unregistering with the server so it can
   * clean up session state first.
   *
   * @param {LOOP_SESSION_TYPE} sessionType The type of session to use for the request.
   *                                        One of the LOOP_SESSION_TYPE members.
   */
  clearSessionToken: function(sessionType) {
    Services.prefs.clearUserPref(this.getSessionTokenPrefName(sessionType));
    log.debug("Cleared hawk session token for sessionType", sessionType);
  },

  /**
   * Callback from MozLoopPushHandler - The push server has been registered
   * and has given us a push url.
   *
   * @param {String} pushUrl The push url given by the push server.
   */
  onPushRegistered: function(err, pushUrl) {
    if (err) {
      gRegisteredDeferred.reject(err);
      gRegisteredDeferred = null;
      return;
    }

    this.registerWithLoopServer(LOOP_SESSION_TYPE.GUEST, pushUrl).then(() => {
      // storeSessionToken could have rejected and nulled the promise if the token was malformed.
      if (!gRegisteredDeferred) {
        return;
      }
      gRegisteredDeferred.resolve();
      // No need to clear the promise here, everything was good, so we don't need
      // to re-register.
    }, (error) => {
      log.error("Failed to register with Loop server: ", error);
      gRegisteredDeferred.reject(error.errno);
      gRegisteredDeferred = null;
    });
  },

  /**
   * Registers with the Loop server either as a guest or a FxA user.
   *
   * @param {LOOP_SESSION_TYPE} sessionType The type of session e.g. guest or FxA
   * @param {String} pushUrl The push url given by the push server.
   * @param {Boolean} [retry=true] Whether to retry if authentication fails.
   * @return {Promise}
   */
  registerWithLoopServer: function(sessionType, pushUrl, retry = true) {
    return this.hawkRequest(sessionType, "/registration", "POST", { simplePushURL: pushUrl})
      .then((response) => {
        // If this failed we got an invalid token. storeSessionToken rejects
        // the gRegisteredDeferred promise for us, so here we just need to
        // early return.
        if (!this.storeSessionToken(sessionType, response.headers))
          return;

        log.debug("Successfully registered with server for sessionType", sessionType);
        this.clearError("registration");
      }, (error) => {
        // There's other errors than invalid auth token, but we should only do the reset
        // as a last resort.
        if (error.code === 401) {
          // Authorization failed, invalid token, we need to try again with a new token.
          if (retry) {
            return this.registerWithLoopServer(sessionType, pushUrl, false);
          }
        }

        log.error("Failed to register with the loop server. Error: ", error);
        this.setError("registration", error);
        throw error;
      }
    );
  },

  /**
   * Unregisters from the Loop server either as a guest or a FxA user.
   *
   * This is normally only wanted for FxA users as we normally want to keep the
   * guest session with the device.
   *
   * @param {LOOP_SESSION_TYPE} sessionType The type of session e.g. guest or FxA
   * @param {String} pushURL The push URL previously given by the push server.
   *                         This may not be necessary to unregister in the future.
   * @return {Promise} resolving when the unregistration request finishes
   */
  unregisterFromLoopServer: function(sessionType, pushURL) {
    let prefType = Services.prefs.getPrefType(this.getSessionTokenPrefName(sessionType));
    if (prefType == Services.prefs.PREF_INVALID) {
      return Promise.resolve("already unregistered");
    }

    let unregisterURL = "/registration?simplePushURL=" + encodeURIComponent(pushURL);
    return this.hawkRequest(sessionType, unregisterURL, "DELETE")
      .then(() => {
        log.debug("Successfully unregistered from server for sessionType", sessionType);
        MozLoopServiceInternal.clearSessionToken(sessionType);
      },
      error => {
        // Always clear the registration token regardless of whether the server acknowledges the logout.
        MozLoopServiceInternal.clearSessionToken(sessionType);
        if (error.code === 401) {
          // Authorization failed, invalid token. This is fine since it may mean we already logged out.
          return;
        }

        log.error("Failed to unregister with the loop server. Error: ", error);
        throw error;
      });
  },

  /**
   * Callback from MozLoopPushHandler - A push notification has been received from
   * the server.
   *
   * @param {String} version The version information from the server.
   */
  onHandleNotification: function(version) {
    if (this.doNotDisturb) {
      return;
    }

    // We set this here as it is assumed that once the user receives an incoming
    // call, they'll have had enough time to see the terms of service. See
    // bug 1046039 for background.
    Services.prefs.setCharPref("loop.seenToS", "seen");

    // Request the information on the new call(s) associated with this version.
    // The registered FxA session is checked first, then the anonymous session.
    // Make the call to get the GUEST session regardless of whether the FXA
    // request fails.

    if (MozLoopService.userProfile) {
      this._getCalls(LOOP_SESSION_TYPE.FXA, version).catch(() => {});
    }
    this._getCalls(LOOP_SESSION_TYPE.GUEST, version).catch(
      error => {this._hawkRequestError(error);});
  },

  /**
   * Make a hawkRequest to GET/calls?=version for this session type.
   *
   * @param {LOOP_SESSION_TYPE} sessionType - type of hawk token used
   *        for the GET operation.
   * @param {Object} version - LoopPushService notification version
   *
   * @returns {Promise}
   *
   */

  _getCalls: function(sessionType, version) {
    return this.hawkRequest(sessionType, "/calls?version=" + version, "GET").then(
      response => {this._processCalls(response, sessionType);}
    );
  },

  /**
   * Process the calls array returned from a GET/calls?version request.
   * Only one active call is permitted at this time.
   *
   * @param {Object} response - response payload from GET
   *
   * @param {LOOP_SESSION_TYPE} sessionType - type of hawk token used
   *        for the GET operation.
   *
   */

  _processCalls: function(response, sessionType) {
    try {
      let respData = JSON.parse(response.body);
      if (respData.calls && Array.isArray(respData.calls)) {
        respData.calls.forEach((callData) => {
          if (!this.callsData.inUse) {
            callData.sessionType = sessionType;
            this._startCall(callData, "incoming");
          } else {
            this._returnBusy(callData);
          }
        });
      } else {
        log.warn("Error: missing calls[] in response");
      }
    } catch (err) {
      log.warn("Error parsing calls info", err);
    }
  },

  /**
   * Starts a call, saves the call data, and opens a chat window.
   *
   * @param {Object} callData The data associated with the call including an id.
   * @param {Boolean} conversationType Whether or not the call is "incoming"
   *                                   or "outgoing"
   */
  _startCall: function(callData, conversationType) {
    this.callsData.inUse = true;
    this.callsData.data = callData;
    this.openChatWindow(
      null,
      // No title, let the page set that, to avoid flickering.
      "",
      "about:loopconversation#" + conversationType + "/" + callData.callId);
  },

  /**
   * Starts a direct call to the contact addresses.
   *
   * @param {Object} contact The contact to call
   * @param {String} callType The type of call, e.g. "audio-video" or "audio-only"
   * @return true if the call is opened, false if it is not opened (i.e. busy)
   */
  startDirectCall: function(contact, callType) {
    if (this.callsData.inUse)
      return false;

    var callData = {
      contact: contact,
      callType: callType,
      callId: Math.floor((Math.random() * 10))
    };

    this._startCall(callData, "outgoing");
    return true;
  },

   /**
   * Open call progress websocket and terminate with a reason of busy
   * the server.
   *
   * @param {callData} Must contain the progressURL, callId and websocketToken
   *                   returned by the LoopService.
   */
  _returnBusy: function(callData) {
    let callProgress = new CallProgressSocket(
      callData.progressURL,
      callData.callId,
      callData.websocketToken);
    callProgress._websocket = this._mocks.webSocket;
    // This instance of CallProgressSocket should stay alive until the underlying
    // websocket is closed since it is passed to the websocket as the nsIWebSocketListener.
    callProgress.connect(() => {callProgress.sendBusy();});
  },

  /**
   * A getter to obtain and store the strings for loop. This is structured
   * for use by l10n.js.
   *
   * @returns {Object} a map of element ids with attributes to set.
   */
  get localizedStrings() {
    if (gLocalizedStrings)
      return gLocalizedStrings;

    var stringBundle =
      Services.strings.createBundle('chrome://browser/locale/loop/loop.properties');

    var map = {};
    var enumerator = stringBundle.getSimpleEnumeration();
    while (enumerator.hasMoreElements()) {
      var string = enumerator.getNext().QueryInterface(Ci.nsIPropertyElement);

      // 'textContent' is the default attribute to set if none are specified.
      var key = string.key, property = 'textContent';
      var i = key.lastIndexOf('.');
      if (i >= 0) {
        property = key.substring(i + 1);
        key = key.substring(0, i);
      }
      if (!(key in map))
        map[key] = {};
      map[key][property] = string.value;
    }

    return gLocalizedStrings = map;
  },

  /**
   * Saves loop logs to the saved-telemetry-pings folder.
   *
   * @param {Object} pc The peerConnection in question.
   */
  stageForTelemetryUpload: function(window, pc) {
    window.WebrtcGlobalInformation.getAllStats(allStats => {
      let internalFormat = allStats.reports[0]; // filtered on pc.id
      window.WebrtcGlobalInformation.getLogging('', logs => {
        let report = convertToRTCStatsReport(internalFormat);
        let logStr = "";
        logs.forEach(s => { logStr += s + "\n"; });

        // We have stats and logs.

        // Create worker job. ping = saved telemetry ping file header + payload
        //
        // Prepare payload according to https://wiki.mozilla.org/Loop/Telemetry

        let ai = Services.appinfo;
        let uuid = uuidgen.generateUUID().toString();
        uuid = uuid.substr(1,uuid.length-2); // remove uuid curly braces

        let directory = OS.Path.join(OS.Constants.Path.profileDir,
                                     "saved-telemetry-pings");
        let job = {
          directory: directory,
          filename: uuid + ".json",
          ping: {
            reason: "loop",
            slug: uuid,
            payload: {
              ver: 1,
              info: {
                appUpdateChannel: ai.defaultUpdateChannel,
                appBuildID: ai.appBuildID,
                appName: ai.name,
                appVersion: ai.version,
                reason: "loop",
                OS: ai.OS,
                version: Services.sysinfo.getProperty("version")
              },
              report: "ice failure",
              connectionstate: pc.iceConnectionState,
              stats: report,
              localSdp: internalFormat.localSdp,
              remoteSdp: internalFormat.remoteSdp,
              log: logStr
            }
          }
        };

        // Send job to worker to do log sanitation, transcoding and saving to
        // disk for pickup by telemetry on next startup, which then uploads it.

        let worker = new ChromeWorker("MozLoopWorker.js");
        worker.onmessage = function(e) {
          log.info(e.data.ok ?
            "Successfully staged loop report for telemetry upload." :
            ("Failed to stage loop report. Error: " + e.data.fail));
        }
        worker.postMessage(job);
      });
    }, pc.id);
  },

  /**
   * Opens the chat window
   *
   * @param {Object} contentWindow The window to open the chat window in, may
   *                               be null.
   * @param {String} title The title of the chat window.
   * @param {String} url The page to load in the chat window.
   */
  openChatWindow: function(contentWindow, title, url) {
    // So I guess the origin is the loop server!?
    let origin = this.loopServerUri;
    url = url.spec || url;

    let callback = chatbox => {
      // We need to use DOMContentLoaded as otherwise the injection will happen
      // in about:blank and then get lost.
      // Sadly we can't use chatbox.promiseChatLoaded() as promise chaining
      // involves event loop spins, which means it might be too late.
      // Have we already done it?
      if (chatbox.contentWindow.navigator.mozLoop) {
        return;
      }

      chatbox.setAttribute("dark", true);

      chatbox.addEventListener("DOMContentLoaded", function loaded(event) {
        if (event.target != chatbox.contentDocument) {
          return;
        }
        chatbox.removeEventListener("DOMContentLoaded", loaded, true);

        let window = chatbox.contentWindow;
        injectLoopAPI(window);

        let ourID = window.QueryInterface(Ci.nsIInterfaceRequestor)
            .getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;

        let onPCLifecycleChange = (pc, winID, type) => {
          if (winID != ourID) {
            return;
          }
          if (type == "iceconnectionstatechange") {
            switch(pc.iceConnectionState) {
              case "failed":
              case "disconnected":
                if (Services.telemetry.canSend ||
                    Services.prefs.getBoolPref("toolkit.telemetry.test")) {
                  this.stageForTelemetryUpload(window, pc);
                }
                break;
            }
          }
        };

        let pc_static = new window.mozRTCPeerConnectionStatic();
        pc_static.registerPeerConnectionLifecycleCallback(onPCLifecycleChange);
      }.bind(this), true);
    };

    Chat.open(contentWindow, origin, title, url, undefined, undefined, callback);
  },

  /**
   * Fetch Firefox Accounts (FxA) OAuth parameters from the Loop Server.
   *
   * @return {Promise} resolved with the body of the hawk request for OAuth parameters.
   */
  promiseFxAOAuthParameters: function() {
    const SESSION_TYPE = LOOP_SESSION_TYPE.FXA;
    return this.hawkRequest(SESSION_TYPE, "/fxa-oauth/params", "POST").then(response => {
      if (!this.storeSessionToken(SESSION_TYPE, response.headers)) {
        throw new Error("Invalid FxA hawk token returned");
      }
      let prefType = Services.prefs.getPrefType(this.getSessionTokenPrefName(SESSION_TYPE));
      if (prefType == Services.prefs.PREF_INVALID) {
        throw new Error("No FxA hawk token returned and we don't have one saved");
      }

      return JSON.parse(response.body);
    },
    error => {this._hawkRequestError(error);});
  },

  /**
   * Get the OAuth client constructed with Loop OAauth parameters.
   *
   * @return {Promise}
   */
  promiseFxAOAuthClient: Task.async(function* () {
    // We must make sure to have only a single client otherwise they will have different states and
    // multiple channels. This would happen if the user clicks the Login button more than once.
    if (gFxAOAuthClientPromise) {
      return gFxAOAuthClientPromise;
    }

    gFxAOAuthClientPromise = this.promiseFxAOAuthParameters().then(
      parameters => {
        try {
          gFxAOAuthClient = new FxAccountsOAuthClient({
            parameters: parameters,
          });
        } catch (ex) {
          gFxAOAuthClientPromise = null;
          throw ex;
        }
        return gFxAOAuthClient;
      },
      error => {
        gFxAOAuthClientPromise = null;
        throw error;
      }
    );

    return gFxAOAuthClientPromise;
  }),

  /**
   * Get the OAuth client and do the authorization web flow to get an OAuth code.
   *
   * @return {Promise}
   */
  promiseFxAOAuthAuthorization: function() {
    let deferred = Promise.defer();
    this.promiseFxAOAuthClient().then(
      client => {
        client.onComplete = this._fxAOAuthComplete.bind(this, deferred);
        client.launchWebFlow();
      },
      error => {
        log.error(error);
        deferred.reject(error);
      }
    );
    return deferred.promise;
  },

  /**
   * Get the OAuth token using the OAuth code and state.
   *
   * The caller should approperiately handle 4xx errors (which should lead to a logout)
   * and 5xx or connectivity issues with messaging to try again later.
   *
   * @param {String} code
   * @param {String} state
   *
   * @return {Promise} resolving with OAuth token data.
   */
  promiseFxAOAuthToken: function(code, state) {
    if (!code || !state) {
      throw new Error("promiseFxAOAuthToken: code and state are required.");
    }

    let payload = {
      code: code,
      state: state,
    };
    return this.hawkRequest(LOOP_SESSION_TYPE.FXA, "/fxa-oauth/token", "POST", payload).then(response => {
      return JSON.parse(response.body);
    },
    error => {this._hawkRequestError(error);});
  },

  /**
   * Called once gFxAOAuthClient fires onComplete.
   *
   * @param {Deferred} deferred used to resolve or reject the gFxAOAuthClientPromise
   * @param {Object} result (with code and state)
   */
  _fxAOAuthComplete: function(deferred, result) {
    gFxAOAuthClientPromise = null;

    // Note: The state was already verified in FxAccountsOAuthClient.
    if (result) {
      deferred.resolve(result);
    } else {
      deferred.reject("Invalid token data");
    }
  },
};
Object.freeze(MozLoopServiceInternal);

let gInitializeTimerFunc = () => {
  // Kick off the push notification service into registering after a timeout
  // this ensures we're not doing too much straight after the browser's finished
  // starting up.
  gInitializeTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  gInitializeTimer.initWithCallback(() => {
    MozLoopService.register();
    gInitializeTimer = null;
  },
  MozLoopServiceInternal.initialRegistrationDelayMilliseconds, Ci.nsITimer.TYPE_ONE_SHOT);
};

/**
 * Public API
 */
this.MozLoopService = {
  _DNSService: gDNSService,

  set initializeTimerFunc(value) {
    gInitializeTimerFunc = value;
  },

  /**
   * Initialized the loop service, and starts registration with the
   * push and loop servers.
   */
  initialize: function() {

    // Do this here, rather than immediately after definition, so that we can
    // stub out API functions for unit testing
    Object.freeze(this);

    // Don't do anything if loop is not enabled.
    if (!Services.prefs.getBoolPref("loop.enabled") ||
        Services.prefs.getBoolPref("loop.throttled")) {
      return;
    }

    if (Services.prefs.getPrefType("loop.fxa.enabled") == Services.prefs.PREF_BOOL) {
      gFxAEnabled = Services.prefs.getBoolPref("loop.fxa.enabled");
      if (!gFxAEnabled) {
        this.logOutFromFxA();
      }
    }

    // If expiresTime is in the future then kick-off registration.
    if (MozLoopServiceInternal.urlExpiryTimeIsInFuture()) {
      gInitializeTimerFunc();
    }
  },

  /**
   * If we're operating the service in "soft start" mode, and this browser
   * isn't already activated, check whether it's time for it to become active.
   * If so, activate the loop service.
   *
   * @param {Object} buttonNode DOM node representing the Loop button -- if we
   *                            change from inactive to active, we need this
   *                            in order to unhide the Loop button.
   * @param {Function} doneCb   [optional] Callback that is called when the
   *                            check has completed.
   */
  checkSoftStart(buttonNode, doneCb) {
    if (!Services.prefs.getBoolPref("loop.throttled")) {
      if (typeof(doneCb) == "function") {
        doneCb(new Error("Throttling is not active"));
      }
      return;
    }

    if (Services.io.offline) {
      if (typeof(doneCb) == "function") {
        doneCb(new Error("Cannot check soft-start value: browser is offline"));
      }
      return;
    }

    let ticket = Services.prefs.getIntPref("loop.soft_start_ticket_number");
    if (!ticket || ticket > MAX_SOFT_START_TICKET_NUMBER || ticket < 0) {
      // Ticket value isn't valid (probably isn't set up yet) -- pick a random
      // number from 1 to MAX_SOFT_START_TICKET_NUMBER, inclusive, and write it
      // into prefs.
      ticket = Math.floor(Math.random() * MAX_SOFT_START_TICKET_NUMBER) + 1;
      // Floating point numbers can be imprecise, so we need to deal with
      // the case that Math.random() effectively rounds to 1.0
      if (ticket > MAX_SOFT_START_TICKET_NUMBER) {
        ticket = MAX_SOFT_START_TICKET_NUMBER;
      }
      Services.prefs.setIntPref("loop.soft_start_ticket_number", ticket);
    }

    let onLookupComplete = (request, record, status) => {
      // We don't bother checking errors -- if the DNS query fails,
      // we just don't activate this time around. We'll check again on
      // next startup.
      if (!Components.isSuccessCode(status)) {
        if (typeof(doneCb) == "function") {
          doneCb(new Error("Error in DNS Lookup: " + status));
        }
        return;
      }

      let address = record.getNextAddrAsString().split(".");
      if (address.length != 4) {
        if (typeof(doneCb) == "function") {
          doneCb(new Error("Invalid IP address"));
        }
        return;
      }

      if (address[0] != 127) {
        if (typeof(doneCb) == "function") {
          doneCb(new Error("Throttling IP address is not on localhost subnet"));
        }
        return
      }

      // Can't use bitwise operations here because JS treats all bitwise
      // operations as 32-bit *signed* integers.
      let now_serving = ((parseInt(address[1]) * 0x10000) +
                         (parseInt(address[2]) * 0x100) +
                         parseInt(address[3]));

      if (now_serving > ticket) {
        // Hot diggity! It's our turn! Activate the service.
        log.info("MozLoopService: Activating Loop via soft-start");
        Services.prefs.setBoolPref("loop.throttled", false);
        buttonNode.hidden = false;
        this.initialize();
      }
      if (typeof(doneCb) == "function") {
        doneCb(null);
      }
    };

    // We use DNS to propagate the slow-start value, since it has well-known
    // scaling properties. Ideally, this would use something more semantic,
    // like a TXT record; but we don't support TXT in our DNS resolution (see
    // Bug 14328), so we instead treat the lowest 24 bits of the IP address
    // corresponding to our "slow start DNS name" as a 24-bit integer. To
    // ensure that these addresses aren't routable, the highest 8 bits must
    // be "127" (reserved for localhost).
    let host = Services.prefs.getCharPref("loop.soft_start_hostname");
    let task = this._DNSService.asyncResolve(host,
                                             this._DNSService.RESOLVE_DISABLE_IPV6,
                                             onLookupComplete,
                                             Services.tm.mainThread);
  },


  /**
   * Starts registration of Loop with the push server, and then will register
   * with the Loop server. It will return early if already registered.
   *
   * @param {Object} mockPushHandler Optional, test-only mock push handler. Used
   *                                 to allow mocking of the MozLoopPushHandler.
   * @returns {Promise} a promise that is resolved with no params on completion, or
   *          rejected with an error code or string.
   */
  register: function(mockPushHandler, mockWebSocket) {
    log.debug("registering");
    // Don't do anything if loop is not enabled.
    if (!Services.prefs.getBoolPref("loop.enabled")) {
      throw new Error("Loop is not enabled");
    }

    if (Services.prefs.getBoolPref("loop.throttled")) {
      throw new Error("Loop is disabled by the soft-start mechanism");
    }

    return MozLoopServiceInternal.promiseRegisteredWithServers(mockPushHandler, mockWebSocket);
  },

  /**
   * Used to note a call url expiry time. If the time is later than the current
   * latest expiry time, then the stored expiry time is increased. For times
   * sooner, this function is a no-op; this ensures we always have the latest
   * expiry time for a url.
   *
   * This is used to deterimine whether or not we should be registering with the
   * push server on start.
   *
   * @param {Integer} expiryTimeSeconds The seconds since epoch of the expiry time
   *                                    of the url.
   */
  noteCallUrlExpiry: function(expiryTimeSeconds) {
    MozLoopServiceInternal.expiryTimeSeconds = expiryTimeSeconds;
  },

  /**
   * Returns the strings for the specified element. Designed for use
   * with l10n.js.
   *
   * @param {key} The element id to get strings for.
   * @return {String} A JSON string containing the localized
   *                  attribute/value pairs for the element.
   */
  getStrings: function(key) {
      var stringData = MozLoopServiceInternal.localizedStrings;
      if (!(key in stringData)) {
        Cu.reportError('No string for key: ' + key + 'found');
        return "";
      }

      return JSON.stringify(stringData[key]);
  },

  /**
   * Returns a new GUID (UUID) in curly braces format.
   */
  generateUUID: function() {
    return uuidgen.generateUUID().toString();
  },

  /**
   * Retrieves MozLoopService "do not disturb" value.
   *
   * @return {Boolean}
   */
  get doNotDisturb() {
    return MozLoopServiceInternal.doNotDisturb;
  },

  /**
   * Sets MozLoopService "do not disturb" value.
   *
   * @param {Boolean} aFlag
   */
  set doNotDisturb(aFlag) {
    MozLoopServiceInternal.doNotDisturb = aFlag;
  },

  get fxAEnabled() {
    return gFxAEnabled;
  },

  get userProfile() {
    return gFxAOAuthProfile;
  },

  get errors() {
    return MozLoopServiceInternal.errors;
  },

  get log() {
    return log;
  },

  /**
   * Returns the current locale
   *
   * @return {String} The code of the current locale.
   */
  get locale() {
    try {
      return Services.prefs.getComplexValue("general.useragent.locale",
        Ci.nsISupportsString).data;
    } catch (ex) {
      return "en-US";
    }
  },

  /**
   * Returns the callData for a specific loopCallId
   *
   * The data was retrieved from the LoopServer via a GET/calls/<version> request
   * triggered by an incoming message from the LoopPushServer.
   *
   * @param {int} loopCallId
   * @return {callData} The callData or undefined if error.
   */
  getCallData: function(loopCallId) {
    if (MozLoopServiceInternal.callsData.data &&
        MozLoopServiceInternal.callsData.data.callId == loopCallId) {
      return MozLoopServiceInternal.callsData.data;
    } else {
      return undefined;
    }
  },

  /**
   * Releases the callData for a specific loopCallId
   *
   * The result of this call will be a free call session slot.
   *
   * @param {int} loopCallId
   */
  releaseCallData: function(loopCallId) {
    if (MozLoopServiceInternal.callsData.data &&
        MozLoopServiceInternal.callsData.data.callId == loopCallId) {
      MozLoopServiceInternal.callsData.data = undefined;
      MozLoopServiceInternal.callsData.inUse = false;
    }
  },

  /**
   * Set any character preference under "loop.".
   *
   * @param {String} prefName The name of the pref without the preceding "loop."
   * @param {String} value The value to set.
   *
   * Any errors thrown by the Mozilla pref API are logged to the console.
   */
  setLoopCharPref: function(prefName, value) {
    try {
      Services.prefs.setCharPref("loop." + prefName, value);
    } catch (ex) {
      log.error("setLoopCharPref had trouble setting " + prefName +
        "; exception: " + ex);
    }
  },

  /**
   * Return any preference under "loop." that's coercible to a character
   * preference.
   *
   * @param {String} prefName The name of the pref without the preceding
   * "loop."
   *
   * Any errors thrown by the Mozilla pref API are logged to the console
   * and cause null to be returned. This includes the case of the preference
   * not being found.
   *
   * @return {String} on success, null on error
   */
  getLoopCharPref: function(prefName) {
    try {
      return Services.prefs.getCharPref("loop." + prefName);
    } catch (ex) {
      log.error("getLoopCharPref had trouble getting " + prefName +
        "; exception: " + ex);
      return null;
    }
  },

  /**
   * Return any preference under "loop." that's coercible to a character
   * preference.
   *
   * @param {String} prefName The name of the pref without the preceding
   * "loop."
   *
   * Any errors thrown by the Mozilla pref API are logged to the console
   * and cause null to be returned. This includes the case of the preference
   * not being found.
   *
   * @return {String} on success, null on error
   */
  getLoopBoolPref: function(prefName) {
    try {
      return Services.prefs.getBoolPref("loop." + prefName);
    } catch (ex) {
      log.error("getLoopBoolPref had trouble getting " + prefName +
        "; exception: " + ex);
      return null;
    }
  },

  /**
   * Start the FxA login flow using the OAuth client and params from the Loop server.
   *
   * The caller should be prepared to handle rejections related to network, server or login errors.
   *
   * @return {Promise} that resolves when the FxA login flow is complete.
   */
  logInToFxA: function() {
    log.debug("logInToFxA with gFxAOAuthTokenData:", !!gFxAOAuthTokenData);
    if (gFxAOAuthTokenData) {
      return Promise.resolve(gFxAOAuthTokenData);
    }

    return MozLoopServiceInternal.promiseFxAOAuthAuthorization().then(response => {
      return MozLoopServiceInternal.promiseFxAOAuthToken(response.code, response.state);
    }).then(tokenData => {
      gFxAOAuthTokenData = tokenData;
      return tokenData;
    }).then(tokenData => {
      return gRegisteredDeferred.promise.then(Task.async(function*() {
        if (gPushHandler.pushUrl) {
          yield MozLoopServiceInternal.registerWithLoopServer(LOOP_SESSION_TYPE.FXA, gPushHandler.pushUrl);
        } else {
          throw new Error("No pushUrl for FxA registration");
        }
        MozLoopServiceInternal.clearError("login");
        MozLoopServiceInternal.clearError("profile");
        return gFxAOAuthTokenData;
      }));
    }).then(tokenData => {
      let client = new FxAccountsProfileClient({
        serverURL: gFxAOAuthClient.parameters.profile_uri,
        token: tokenData.access_token
      });
      client.fetchProfile().then(result => {
        gFxAOAuthProfile = result;
        MozLoopServiceInternal.notifyStatusChanged("login");
      }, error => {
        log.error("Failed to retrieve profile", error);
        this.setError("profile", error);
        gFxAOAuthProfile = null;
        MozLoopServiceInternal.notifyStatusChanged();
      });
      return tokenData;
    }).catch(error => {
      gFxAOAuthTokenData = null;
      gFxAOAuthProfile = null;
      throw error;
    }).catch((error) => {
      MozLoopServiceInternal.setError("login", error);
      // Re-throw for testing
      throw error;
    });
  },

  /**
   * Logs the user out from FxA.
   *
   * Gracefully handles if the user is already logged out.
   *
   * @return {Promise} that resolves when the FxA logout flow is complete.
   */
  logOutFromFxA: Task.async(function*() {
    log.debug("logOutFromFxA");
    if (gPushHandler && gPushHandler.pushUrl) {
      yield MozLoopServiceInternal.unregisterFromLoopServer(LOOP_SESSION_TYPE.FXA,
                                                            gPushHandler.pushUrl);
    } else {
      MozLoopServiceInternal.clearSessionToken(LOOP_SESSION_TYPE.FXA);
    }

    gFxAOAuthTokenData = null;
    gFxAOAuthProfile = null;

    // Reset the client since the initial promiseFxAOAuthParameters() call is
    // what creates a new session.
    gFxAOAuthClient = null;
    gFxAOAuthClientPromise = null;

    // clearError calls notifyStatusChanged so should be done last when the
    // state is clean.
    MozLoopServiceInternal.clearError("registration");
    MozLoopServiceInternal.clearError("login");
    MozLoopServiceInternal.clearError("profile");
  }),

  openFxASettings: function() {
    let url = new URL("/settings", gFxAOAuthClient.parameters.content_uri);
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    win.switchToTabHavingURI(url.toString(), true);
  },

  /**
   * Performs a hawk based request to the loop server.
   *
   * @param {LOOP_SESSION_TYPE} sessionType The type of session to use for the request.
   *                                        One of the LOOP_SESSION_TYPE members.
   * @param {String} path The path to make the request to.
   * @param {String} method The request method, e.g. 'POST', 'GET'.
   * @param {Object} payloadObj An object which is converted to JSON and
   *                            transmitted with the request.
   * @returns {Promise}
   *        Returns a promise that resolves to the response of the API call,
   *        or is rejected with an error.  If the server response can be parsed
   *        as JSON and contains an 'error' property, the promise will be
   *        rejected with this JSON-parsed response.
   */
  hawkRequest: function(sessionType, path, method, payloadObj) {
    return MozLoopServiceInternal.hawkRequest(sessionType, path, method, payloadObj).catch(
      error => {MozLoopServiceInternal._hawkRequestError(error);});
  },

    /**
     * Starts a direct call to the contact addresses.
     *
     * @param {Object} contact The contact to call
     * @param {String} callType The type of call, e.g. "audio-video" or "audio-only"
     * @return true if the call is opened, false if it is not opened (i.e. busy)
     */
  startDirectCall: function(contact, callType) {
    MozLoopServiceInternal.startDirectCall(contact, callType);
  },
};
