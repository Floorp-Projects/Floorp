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

const EMAIL_OR_PHONE_RE = /^(:?\S+@\S+|\+\d+)$/;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/osfile.jsm", this);
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
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

XPCOMUtils.defineLazyModuleGetter(this, "LoopContacts",
                                  "resource:///modules/loop/LoopContacts.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LoopStorage",
                                  "resource:///modules/loop/LoopStorage.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LoopCalls",
                                  "resource:///modules/loop/LoopCalls.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LoopRooms",
                                  "resource:///modules/loop/LoopRooms.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "roomsPushNotification",
                                  "resource:///modules/loop/LoopRooms.jsm");

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

function setJSONPref(aName, aValue) {
  let value = !!aValue ? JSON.stringify(aValue) : "";
  Services.prefs.setCharPref(aName, value);
}

function getJSONPref(aName) {
  let value = Services.prefs.getCharPref(aName);
  return !!value ? JSON.parse(value) : null;
}

let gHawkClient = null;
let gLocalizedStrings = null;
let gFxAEnabled = true;
let gFxAOAuthClientPromise = null;
let gFxAOAuthClient = null;
let gErrors = new Map();
let gLastWindowId = 0;
let gConversationWindowData = new Map();

/**
 * Internal helper methods and state
 *
 * The registration is a two-part process. First we need to connect to
 * and register with the push server. Then we need to take the result of that
 * and register with the Loop server.
 */
let MozLoopServiceInternal = {
  mocks: {
    pushHandler: undefined,
    webSocket: undefined,
  },

  /**
   * The current deferreds for the registration processes. This is set if in progress
   * or the registration was successful. This is null if a registration attempt was
   * unsuccessful.
   */
  deferredRegistrations: new Map(),

  get pushHandler() this.mocks.pushHandler || MozLoopPushHandler,

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
   * Retrieves MozLoopService Firefox Accounts OAuth token.
   *
   * @return {Object} OAuth token
   */
  get fxAOAuthTokenData() {
    return getJSONPref("loop.fxa_oauth.tokendata");
  },

  /**
   * Sets MozLoopService Firefox Accounts OAuth token.
   * If the tokenData is being cleared, will also clear the
   * profile since the profile is dependent on the token data.
   *
   * @param {Object} aTokenData OAuth token
   */
  set fxAOAuthTokenData(aTokenData) {
    setJSONPref("loop.fxa_oauth.tokendata", aTokenData);
    if (!aTokenData) {
      this.fxAOAuthProfile = null;
    }
  },

  /**
   * Sets MozLoopService Firefox Accounts Profile data.
   *
   * @param {Object} aProfileData Profile data
   */
  set fxAOAuthProfile(aProfileData) {
    setJSONPref("loop.fxa_oauth.profile", aProfileData);
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
    let profile = MozLoopService.userProfile;
    LoopStorage.switchDatabase(profile ? profile.uid : null);
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
   * Get endpoints with the push server and register for notifications.
   * For now we register as both a Guest and FxA user and all must succeed.
   *
   * @return {Promise} resolves with all push endpoints
   *                   rejects if any of the push registrations failed
   */
  promiseRegisteredWithPushServer: function() {
    // Wrap push notification registration call-back in a Promise.
    function registerForNotification(channelID, onNotification) {
      log.debug("registerForNotification", channelID);
      return new Promise((resolve, reject) => {
        function onRegistered(error, pushUrl) {
          log.debug("registerForNotification onRegistered:", error, pushUrl);
          if (error) {
            reject(Error(error));
          } else {
            resolve(pushUrl);
          }
        }

        // If we're already registered, resolve with the existing push URL
        let pushURL = MozLoopServiceInternal.pushHandler.registeredChannels[channelID];
        if (pushURL) {
          log.debug("Using the existing push endpoint for channelID:", channelID);
          resolve(pushURL);
          return;
        }

        MozLoopServiceInternal.pushHandler.register(channelID, onRegistered, onNotification);
      });
    }

    let options = this.mocks.webSocket ? { mockWebSocket: this.mocks.webSocket } : {};
    this.pushHandler.initialize(options);

    let callsRegGuest = registerForNotification(MozLoopService.channelIDs.callsGuest,
                                                LoopCalls.onNotification);

    let roomsRegGuest = registerForNotification(MozLoopService.channelIDs.roomsGuest,
                                                roomsPushNotification);

    let callsRegFxA = registerForNotification(MozLoopService.channelIDs.callsFxA,
                                              LoopCalls.onNotification);

    let roomsRegFxA = registerForNotification(MozLoopService.channelIDs.roomsFxA,
                                              roomsPushNotification);

    return Promise.all([callsRegGuest, roomsRegGuest, callsRegFxA, roomsRegFxA]);
  },

  /**
   * Starts registration of Loop with the push server, and then will register
   * with the Loop server. It will return early if already registered.
   *
   * @param {LOOP_SESSION_TYPE} sessionType
   * @returns {Promise} a promise that is resolved with no params on completion, or
   *          rejected with an error code or string.
   */
  promiseRegisteredWithServers: function(sessionType = LOOP_SESSION_TYPE.GUEST) {
    if (this.deferredRegistrations.has(sessionType)) {
      log.debug("promiseRegisteredWithServers: registration already completed or in progress:", sessionType);
      return this.deferredRegistrations.get(sessionType).promise;
    }

    let result = null;
    let deferred = Promise.defer();
    log.debug("assigning to deferredRegistrations for sessionType:", sessionType);
    this.deferredRegistrations.set(sessionType, deferred);

    // We grab the promise early in case one of the callers below delete it from the map.
    result = deferred.promise;

    this.promiseRegisteredWithPushServer().then(() => {
      return this.registerWithLoopServer(sessionType);
    }).then(() => {
      deferred.resolve("registered to status:" + sessionType);
      // No need to clear the promise here, everything was good, so we don't need
      // to re-register.
    }, error => {
      log.error("Failed to register with Loop server with sessionType " + sessionType, error);
      deferred.reject(error);
      this.deferredRegistrations.delete(sessionType);
      log.debug("Cleared deferredRegistration for sessionType:", sessionType);
    });

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
    log.debug("hawkRequest: " + path, sessionType);
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
   * Registers with the Loop server either as a guest or a FxA user. This method should only be
   * called by promiseRegisteredWithServers since it prevents calling this while a registration is
   * already in progress.
   *
   * @private
   * @param {LOOP_SESSION_TYPE} sessionType The type of session e.g. guest or FxA
   * @param {Boolean} [retry=true] Whether to retry if authentication fails.
   * @return {Promise}
   */
  registerWithLoopServer: function(sessionType, retry = true) {
    log.debug("registerWithLoopServer with sessionType:", sessionType);

    let callsPushURL, roomsPushURL;
    if (sessionType == LOOP_SESSION_TYPE.FXA) {
      callsPushURL = this.pushHandler.registeredChannels[MozLoopService.channelIDs.callsFxA];
      roomsPushURL = this.pushHandler.registeredChannels[MozLoopService.channelIDs.roomsFxA];
    } else if (sessionType == LOOP_SESSION_TYPE.GUEST) {
      callsPushURL = this.pushHandler.registeredChannels[MozLoopService.channelIDs.callsGuest];
      roomsPushURL = this.pushHandler.registeredChannels[MozLoopService.channelIDs.roomsGuest];
    }

    if (!callsPushURL || !roomsPushURL) {
      return Promise.reject("Invalid sessionType or missing push URLs for registerWithLoopServer: " + sessionType);
    }

    // create a registration payload with a backwards compatible attribute (simplePushURL)
    // that will register only the calls notification.
    let msg = {
        simplePushURL: callsPushURL,
        simplePushURLs: {
          calls: callsPushURL,
          rooms: roomsPushURL,
        },
    };
    return this.hawkRequest(sessionType, "/registration", "POST", msg)
      .then((response) => {
        // If this failed we got an invalid token.
        if (!this.storeSessionToken(sessionType, response.headers)) {
          return Promise.reject("session-token-wrong-size");
        }

        log.debug("Successfully registered with server for sessionType", sessionType);
        this.clearError("registration");
        return undefined;
      }, (error) => {
        // There's other errors than invalid auth token, but we should only do the reset
        // as a last resort.
        if (error.code === 401) {
          // Authorization failed, invalid token, we need to try again with a new token.
          if (retry) {
            return this.registerWithLoopServer(sessionType, false);
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
   * NOTE: It is the responsibiliy of the caller the clear the session token
   * after all of the notification classes: calls and rooms, for either
   * Guest or FxA have been unregistered with the LoopServer.
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
      },
      error => {
        if (error.code === 401) {
          // Authorization failed, invalid token. This is fine since it may mean we already logged out.
          return;
        }

        log.error("Failed to unregister with the loop server. Error: ", error);
        throw error;
      });
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
   * @param {Object} conversationWindowData The data to be obtained by the
   *                                        window when it opens.
   * @returns {Number} The id of the window.
   */
  openChatWindow: function(conversationWindowData) {
    // So I guess the origin is the loop server!?
    let origin = this.loopServerUri;
    let windowId = gLastWindowId++;
    // Store the id as a string, as that's what we use elsewhere.
    windowId = windowId.toString();

    gConversationWindowData.set(windowId, conversationWindowData);

    let url = "about:loopconversation#" + windowId;

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

    Chat.open(null, origin, "", url, undefined, undefined, callback);
    return windowId;
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


let gInitializeTimerFunc = (deferredInitialization) => {
  // Kick off the push notification service into registering after a timeout.
  // This ensures we're not doing too much straight after the browser's finished
  // starting up.

  setTimeout(MozLoopService.delayedInitialize.bind(MozLoopService, deferredInitialization),
             MozLoopServiceInternal.initialRegistrationDelayMilliseconds);
};


/**
 * Public API
 */
this.MozLoopService = {
  _DNSService: gDNSService,

  get channelIDs() {
    // Channel ids that will be registered with the PushServer for notifications
    return {
      callsFxA: "25389583-921f-4169-a426-a4673658944b",
      callsGuest: "801f754b-686b-43ec-bd83-1419bbf58388",
      roomsFxA: "6add272a-d316-477c-8335-f00f73dfde71",
      roomsGuest: "19d3f799-a8f3-4328-9822-b7cd02765832",
    };
  },


  set initializeTimerFunc(value) {
    gInitializeTimerFunc = value;
  },

  /**
   * Initialized the loop service, and starts registration with the
   * push and loop servers.
   *
   * @return {Promise}
   */
  initialize: Task.async(function*() {
    // Do this here, rather than immediately after definition, so that we can
    // stub out API functions for unit testing
    Object.freeze(this);

    // Clear the old throttling mechanism.
    Services.prefs.clearUserPref("loop.throttled");

    // Don't do anything if loop is not enabled.
    if (!Services.prefs.getBoolPref("loop.enabled") ||
        Services.prefs.getBoolPref("loop.throttled2")) {
      return Promise.reject("loop is not enabled");
    }

    if (Services.prefs.getPrefType("loop.fxa.enabled") == Services.prefs.PREF_BOOL) {
      gFxAEnabled = Services.prefs.getBoolPref("loop.fxa.enabled");
      if (!gFxAEnabled) {
        yield this.logOutFromFxA();
      }
    }

    // If expiresTime is not in the future and the user hasn't
    // previously authenticated then skip registration.
    if (!MozLoopServiceInternal.urlExpiryTimeIsInFuture() &&
        !MozLoopServiceInternal.fxAOAuthTokenData) {
      return Promise.resolve("registration not needed");
    }

    let deferredInitialization = Promise.defer();
    gInitializeTimerFunc(deferredInitialization);

    return deferredInitialization.promise;
  }),

  /**
   * The core of the initialization work that happens once the browser is ready
   * (after a timer when called during startup).
   *
   * Can be called more than once (e.g. if the initial setup fails at some phase).
   * @param {Deferred} deferredInitialization
   */
  delayedInitialize: Task.async(function*(deferredInitialization) {
    // Set or clear an error depending on how deferredInitialization gets resolved.
    // We do this first so that it can handle the early returns below.
    let completedPromise = deferredInitialization.promise.then(result => {
      MozLoopServiceInternal.clearError("initialization");
      return result;
    },
    error => {
      // If we get a non-object then setError was already called for a different error type.
      if (typeof(error) == "object") {
        MozLoopServiceInternal.setError("initialization", error);
      }
    });

    try {
      if (MozLoopServiceInternal.urlExpiryTimeIsInFuture()) {
        yield this.promiseRegisteredWithServers(LOOP_SESSION_TYPE.GUEST);
      } else {
        log.debug("delayedInitialize: URL expiry time isn't in the future so not registering as a guest");
      }
    } catch (ex) {
      log.debug("MozLoopService: Failure of guest registration", ex);
      deferredInitialization.reject(ex);
      yield completedPromise;
      return;
    }

    if (!MozLoopServiceInternal.fxAOAuthTokenData) {
      log.debug("delayedInitialize: Initialized without an already logged-in account");
      deferredInitialization.resolve("initialized without FxA status");
      yield completedPromise;
      return;
    }

    log.debug("MozLoopService: Initializing with already logged-in account");
    MozLoopServiceInternal.promiseRegisteredWithServers(LOOP_SESSION_TYPE.FXA).then(() => {
      deferredInitialization.resolve("initialized to logged-in status");
    }, error => {
      log.debug("MozLoopService: error logging in using cached auth token");
      MozLoopServiceInternal.setError("login", error);
      deferredInitialization.reject("error logging in using cached auth token");
    });
    yield completedPromise;
  }),

  /**
   * Opens the chat window
   *
   * @param {Object} conversationWindowData The data to be obtained by the
   *                                        window when it opens.
   * @returns {Number} The id of the window.
   */
  openChatWindow: function(conversationWindowData) {
    return MozLoopServiceInternal.openChatWindow(conversationWindowData);
  },

  /**
   * If we're operating the service in "soft start" mode, and this browser
   * isn't already activated, check whether it's time for it to become active.
   * If so, activate the loop service.
   *
   * @param {Function} doneCb   [optional] Callback that is called when the
   *                            check has completed.
   */
  checkSoftStart(doneCb) {
    if (!Services.prefs.getBoolPref("loop.throttled2")) {
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
        Services.prefs.setBoolPref("loop.throttled2", false);
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
   * @see MozLoopServiceInternal.promiseRegisteredWithServers
   */
  promiseRegisteredWithServers: function(sessionType = LOOP_SESSION_TYPE.GUEST) {
    return MozLoopServiceInternal.promiseRegisteredWithServers(sessionType);
  },

  /**
   * Used to note a call url expiry time. If the time is later than the current
   * latest expiry time, then the stored expiry time is increased. For times
   * sooner, this function is a no-op; this ensures we always have the latest
   * expiry time for a url.
   *
   * This is used to determine whether or not we should be registering with the
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
        log.error("No string found for key: ", key);
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
   * Returns a new non-global id
   *
   * @param {Function} notUnique [optional] This function will be
   *                   applied to test the generated id for uniqueness
   *                   in the callers domain.
   */
  generateLocalID: function(notUnique = ((id) => {return false})) {
    do {
      var id = Date.now().toString(36) + Math.floor((Math.random() * 4096)).toString(16);
    }
    while (notUnique(id));
    return id;
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

  /**
   * Gets the user profile, but only if there is
   * tokenData present. Without tokenData, the
   * profile is meaningless.
   *
   * @return {Object}
   */
  get userProfile() {
    return getJSONPref("loop.fxa_oauth.tokendata") &&
           getJSONPref("loop.fxa_oauth.profile");
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
    log.debug("logInToFxA with fxAOAuthTokenData:", !!MozLoopServiceInternal.fxAOAuthTokenData);
    if (MozLoopServiceInternal.fxAOAuthTokenData) {
      return Promise.resolve(MozLoopServiceInternal.fxAOAuthTokenData);
    }
    return MozLoopServiceInternal.promiseFxAOAuthAuthorization().then(response => {
      return MozLoopServiceInternal.promiseFxAOAuthToken(response.code, response.state);
    }).then(tokenData => {
      MozLoopServiceInternal.fxAOAuthTokenData = tokenData;
      return tokenData;
    }).then(tokenData => {
      return MozLoopServiceInternal.promiseRegisteredWithServers(LOOP_SESSION_TYPE.FXA).then(() => {
        MozLoopServiceInternal.clearError("login");
        MozLoopServiceInternal.clearError("profile");
        return MozLoopServiceInternal.fxAOAuthTokenData;
      });
    }).then(tokenData => {
      let client = new FxAccountsProfileClient({
        serverURL: gFxAOAuthClient.parameters.profile_uri,
        token: tokenData.access_token
      });
      client.fetchProfile().then(result => {
        MozLoopServiceInternal.fxAOAuthProfile = result;
        MozLoopServiceInternal.notifyStatusChanged("login");
      }, error => {
        log.error("Failed to retrieve profile", error);
        this.setError("profile", error);
        MozLoopServiceInternal.fxAOAuthProfile = null;
        MozLoopServiceInternal.notifyStatusChanged();
      });
      return tokenData;
    }).catch(error => {
      MozLoopServiceInternal.fxAOAuthTokenData = null;
      MozLoopServiceInternal.fxAOAuthProfile = null;
      MozLoopServiceInternal.deferredRegistrations.delete(LOOP_SESSION_TYPE.FXA);
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
    let pushHandler = MozLoopServiceInternal.pushHandler;
    let callsPushUrl = pushHandler.registeredChannels[MozLoopService.channelIDs.callsFxA];
    let roomsPushUrl = pushHandler.registeredChannels[MozLoopService.channelIDs.roomsFxA];
    try {
      if (callsPushUrl) {
        yield MozLoopServiceInternal.unregisterFromLoopServer(LOOP_SESSION_TYPE.FXA, callsPushUrl);
      }
      if (roomsPushUrl) {
        yield MozLoopServiceInternal.unregisterFromLoopServer(LOOP_SESSION_TYPE.FXA, roomsPushUrl);
      }
    } catch (error) {
      throw error;
    } finally {
      MozLoopServiceInternal.clearSessionToken(LOOP_SESSION_TYPE.FXA);

      MozLoopServiceInternal.fxAOAuthTokenData = null;
      MozLoopServiceInternal.fxAOAuthProfile = null;
      MozLoopServiceInternal.deferredRegistrations.delete(LOOP_SESSION_TYPE.FXA);

      // Reset the client since the initial promiseFxAOAuthParameters() call is
      // what creates a new session.
      gFxAOAuthClient = null;
      gFxAOAuthClientPromise = null;

      // clearError calls notifyStatusChanged so should be done last when the
      // state is clean.
      MozLoopServiceInternal.clearError("registration");
      MozLoopServiceInternal.clearError("login");
      MozLoopServiceInternal.clearError("profile");
    }
  }),

  openFxASettings: Task.async(function() {
    try {
      let fxAOAuthClient = yield MozLoopServiceInternal.promiseFxAOAuthClient();
      if (!fxAOAuthClient) {
        log.error("Could not get the OAuth client");
        return;
      }
      let url = new URL("/settings", fxAOAuthClient.parameters.content_uri);
      let win = Services.wm.getMostRecentWindow("navigator:browser");
      win.switchToTabHavingURI(url.toString(), true);
    } catch (ex) {
      log.error("Error opening FxA settings", ex);
    }
  }),

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
   * Returns the window data for a specific conversation window id.
   *
   * This data will be relevant to the type of window, e.g. rooms or calls.
   * See LoopRooms or LoopCalls for more information.
   *
   * @param {String} conversationWindowId
   * @returns {Object} The window data or null if error.
   */
  getConversationWindowData: function(conversationWindowId) {
    if (gConversationWindowData.has(conversationWindowId)) {
      var conversationData = gConversationWindowData.get(conversationWindowId);
      gConversationWindowData.delete(conversationWindowId);
      return conversationData;
    }

    log.error("Window data was already fetched before. Possible race condition!");
    return null;
  }
};
