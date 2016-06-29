/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";var _typeof = typeof Symbol === "function" && typeof Symbol.iterator === "symbol" ? function (obj) {return typeof obj;} : function (obj) {return obj && typeof Symbol === "function" && obj.constructor === Symbol ? "symbol" : typeof obj;};function _toConsumableArray(arr) {if (Array.isArray(arr)) {for (var i = 0, arr2 = Array(arr.length); i < arr.length; i++) {arr2[i] = arr[i];}return arr2;} else {return Array.from(arr);}}var _Components = 

Components;var Ci = _Components.interfaces;var Cu = _Components.utils;var Cr = _Components.results;

var LOOP_SESSION_TYPE = { 
  GUEST: 1, 
  FXA: 2 };


/**
 * Values that we segment sharing a room URL action telemetry probes into.
 *
 * @type {{COPY_FROM_PANEL: Number, COPY_FROM_CONVERSATION: Number,
 *    EMAIL_FROM_CALLFAILED: Number, EMAIL_FROM_CONVERSATION: Number,
 *    FACEBOOK_FROM_CONVERSATION: Number, EMAIL_FROM_PANEL: Number}}
 */
var SHARING_ROOM_URL = { 
  COPY_FROM_PANEL: 0, 
  COPY_FROM_CONVERSATION: 1, 
  EMAIL_FROM_CALLFAILED: 2, 
  EMAIL_FROM_CONVERSATION: 3, 
  FACEBOOK_FROM_CONVERSATION: 4, 
  EMAIL_FROM_PANEL: 5 };


/**
 * Values that we segment room create action telemetry probes into.
 *
 * @type {{CREATE_SUCCESS: Number, CREATE_FAIL: Number}}
 */
var ROOM_CREATE = { 
  CREATE_SUCCESS: 0, 
  CREATE_FAIL: 1 };


/**
 * Values that we segment copy panel action telemetry probes into.
 *
 * @enum {Number}
 */
var COPY_PANEL = { 
  // Copy panel was shown to the user.
  SHOWN: 0, 
  // User selected "no" and to allow the panel to show again.
  NO_AGAIN: 1, 
  // User selected "no" and to never show the panel.
  NO_NEVER: 2, 
  // User selected "yes" and to allow the panel to show again.
  YES_AGAIN: 3, 
  // User selected "yes" and to never show the panel.
  YES_NEVER: 4 };


/**
* Values that we segment MAUs telemetry probes into.
*
* @type {{OPEN_PANEL: Number, OPEN_CONVERSATION: Number,
*        ROOM_OPEN: Number, ROOM_SHARE: Number, ROOM_DELETE: Number}}
*/
var LOOP_MAU_TYPE = { 
  OPEN_PANEL: 0, 
  OPEN_CONVERSATION: 1, 
  ROOM_OPEN: 2, 
  ROOM_SHARE: 3, 
  ROOM_DELETE: 4 };


// See LOG_LEVELS in Console.jsm. Common examples: "All", "Info", "Warn", & "Error".
var PREF_LOG_LEVEL = "loop.debug.loglevel";

var kChatboxHangupButton = { 
  id: "loop-hangup", 
  visibleWhenUndocked: false, 
  onCommand: function onCommand(e, chatbox) {
    var mm = chatbox.content.messageManager;
    mm.sendAsyncMessage("Social:CustomEvent", { name: "LoopHangupNow" });} };



Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/osfile.jsm", this);
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/FxAccountsOAuthClient.jsm");

Cu.importGlobalProperties(["URL"]);

this.EXPORTED_SYMBOLS = ["MozLoopService", "LOOP_SESSION_TYPE", "LOOP_MAU_TYPE", 
"SHARING_ROOM_URL", "COPY_PANEL", "ROOM_CREATE"];

XPCOMUtils.defineConstant(this, "LOOP_SESSION_TYPE", LOOP_SESSION_TYPE);
XPCOMUtils.defineConstant(this, "SHARING_ROOM_URL", SHARING_ROOM_URL);
XPCOMUtils.defineConstant(this, "COPY_PANEL", COPY_PANEL);
XPCOMUtils.defineConstant(this, "ROOM_CREATE", ROOM_CREATE);
XPCOMUtils.defineConstant(this, "LOOP_MAU_TYPE", LOOP_MAU_TYPE);

XPCOMUtils.defineLazyModuleGetter(this, "LoopAPI", 
"chrome://loop/content/modules/MozLoopAPI.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "convertToRTCStatsReport", 
"resource://gre/modules/media/RTCStatsReport.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "loopUtils", 
"chrome://loop/content/modules/utils.js", "utils");
XPCOMUtils.defineLazyModuleGetter(this, "loopCrypto", 
"chrome://loop/content/shared/js/crypto.js", "LoopCrypto");

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

XPCOMUtils.defineLazyModuleGetter(this, "LoopRooms", 
"chrome://loop/content/modules/LoopRooms.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "roomsPushNotification", 
"chrome://loop/content/modules/LoopRooms.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "MozLoopPushHandler", 
"chrome://loop/content/modules/MozLoopPushHandler.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "UITour", 
"resource:///modules/UITour.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "uuidgen", 
"@mozilla.org/uuid-generator;1", 
"nsIUUIDGenerator");

XPCOMUtils.defineLazyServiceGetter(this, "gDNSService", 
"@mozilla.org/network/dns-service;1", 
"nsIDNSService");

XPCOMUtils.defineLazyServiceGetter(this, "gWM", 
"@mozilla.org/appshell/window-mediator;1", 
"nsIWindowMediator");

// Create a new instance of the ConsoleAPI so we can control the maxLogLevel with a pref.
XPCOMUtils.defineLazyGetter(this, "log", function () {
  var ConsoleAPI = Cu.import("resource://gre/modules/Console.jsm", {}).ConsoleAPI;
  var consoleOptions = { 
    maxLogLevelPref: PREF_LOG_LEVEL, 
    prefix: "Loop" };

  return new ConsoleAPI(consoleOptions);});


function setJSONPref(aName, aValue) {
  var value = aValue ? JSON.stringify(aValue) : "";
  Services.prefs.setCharPref(aName, value);}


function getJSONPref(aName) {
  var value = Services.prefs.getCharPref(aName);
  return value ? JSON.parse(value) : null;}


var gHawkClient = null;
var gLocalizedStrings = new Map();
var gFxAOAuthClientPromise = null;
var gFxAOAuthClient = null;
var gErrors = new Map();
var gConversationWindowData = new Map();
var gAddonVersion = "unknown";

/**
 * Internal helper methods and state
 *
 * The registration is a two-part process. First we need to connect to
 * and register with the push server. Then we need to take the result of that
 * and register with the Loop server.
 */
var MozLoopServiceInternal = { 
  conversationContexts: new Map(), 
  pushURLs: new Map(), 

  mocks: { 
    pushHandler: undefined, 
    isChatWindowOpen: undefined }, 


  /**
   * The current deferreds for the registration processes. This is set if in progress
   * or the registration was successful. This is null if a registration attempt was
   * unsuccessful.
   */
  deferredRegistrations: new Map(), 

  get pushHandler() {
    return this.mocks.pushHandler || MozLoopPushHandler;}, 


  // The uri of the Loop server.
  get loopServerUri() {
    return Services.prefs.getCharPref("loop.server");}, 


  /**
   * The initial delay for push registration. This ensures we don't start
   * kicking off straight after browser startup, just a few seconds later.
   */
  get initialRegistrationDelayMilliseconds() {
    try {
      // Let a pref override this for developer & testing use.
      return Services.prefs.getIntPref("loop.initialDelay");} 
    catch (x) {
      // Default to 5 seconds
      return 5000;}}, 



  /**
   * Retrieves MozLoopService Firefox Accounts OAuth token.
   *
   * @return {Object} OAuth token
   */
  get fxAOAuthTokenData() {
    return getJSONPref("loop.fxa_oauth.tokendata");}, 


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
      this.fxAOAuthProfile = null;}}, 



  /**
   * Sets MozLoopService Firefox Accounts Profile data.
   *
   * @param {Object} aProfileData Profile data
   */
  set fxAOAuthProfile(aProfileData) {
    setJSONPref("loop.fxa_oauth.profile", aProfileData);
    this.notifyStatusChanged(aProfileData ? "login" : undefined);}, 


  /**
   * Retrieves MozLoopService "do not disturb" pref value.
   *
   * @return {Boolean} aFlag
   */
  get doNotDisturb() {
    return Services.prefs.getBoolPref("loop.do_not_disturb");}, 


  /**
   * Sets MozLoopService "do not disturb" pref value.
   *
   * @param {Boolean} aFlag
   */
  set doNotDisturb(aFlag) {
    Services.prefs.setBoolPref("loop.do_not_disturb", Boolean(aFlag));
    this.notifyStatusChanged();}, 


  notifyStatusChanged: function notifyStatusChanged() {var aReason = arguments.length <= 0 || arguments[0] === undefined ? null : arguments[0];
    log.debug("notifyStatusChanged with reason:", aReason);
    var profile = MozLoopService.userProfile;
    LoopRooms.maybeRefresh(profile && profile.uid);
    Services.obs.notifyObservers(null, "loop-status-changed", aReason);}, 


  /**
   * Record an error and notify interested UI with the relevant user-facing strings attached.
   *
   * @param {String} errorType a key to identify the type of error. Only one
   *                           error of a type will be saved at a time. This value may be used to
   *                           determine user-facing (aka. friendly) strings.
   * @param {Object} error     an object describing the error in the format from Hawk errors
   * @param {Function} [actionCallback] an object describing the label and callback function for error
   *                                    bar's button e.g. to retry.
   */
  setError: function setError(errorType, error) {var actionCallback = arguments.length <= 2 || arguments[2] === undefined ? null : arguments[2];
    log.debug("setError", errorType, error);
    log.trace();
    var messageString = void 0, detailsString = void 0, detailsButtonLabelString = void 0, detailsButtonCallback = void 0;
    var NETWORK_ERRORS = [
    Cr.NS_ERROR_CONNECTION_REFUSED, 
    Cr.NS_ERROR_NET_INTERRUPT, 
    Cr.NS_ERROR_NET_RESET, 
    Cr.NS_ERROR_NET_TIMEOUT, 
    Cr.NS_ERROR_OFFLINE, 
    Cr.NS_ERROR_PROXY_CONNECTION_REFUSED, 
    Cr.NS_ERROR_UNKNOWN_HOST, 
    Cr.NS_ERROR_UNKNOWN_PROXY_HOST];


    if (error.code === null && error.errno === null && 
    error.error instanceof Ci.nsIException && 
    NETWORK_ERRORS.indexOf(error.error.result) != -1) {
      // Network error. Override errorType so we can easily clear it on the next succesful request.
      errorType = "network";
      messageString = "could_not_connect";
      detailsString = "check_internet_connection";
      detailsButtonLabelString = "retry_button";} else 
    if (errorType == "profile" && error.code >= 500 && error.code < 600) {
      messageString = "problem_accessing_account";} else 
    if (error.code == 401) {
      if (errorType == "login") {
        messageString = "could_not_authenticate"; // XXX: Bug 1076377
        detailsString = "password_changed_question";
        detailsButtonLabelString = "retry_button";
        detailsButtonCallback = function detailsButtonCallback() {return MozLoopService.logInToFxA();};} else 
      {
        messageString = "session_expired_error_description";}} else 

    if (error.code >= 500 && error.code < 600) {
      messageString = "service_not_available";
      detailsString = "try_again_later";
      detailsButtonLabelString = "retry_button";} else 
    {
      messageString = "generic_failure_message";}


    error.friendlyMessage = this.localizedStrings.get(messageString);

    // Default to the generic "retry_button" text even though the button won't be shown if
    // error.friendlyDetails is null.
    error.friendlyDetailsButtonLabel = detailsButtonLabelString ? 
    this.localizedStrings.get(detailsButtonLabelString) : 
    this.localizedStrings.get("retry_button");

    error.friendlyDetailsButtonCallback = actionCallback || detailsButtonCallback || null;

    if (detailsString) {
      error.friendlyDetails = this.localizedStrings.get(detailsString);} else 
    if (error.friendlyDetailsButtonCallback) {
      // If we have a retry callback but no details use the generic try again string.
      error.friendlyDetails = this.localizedStrings.get("generic_failure_no_reason2");} else 
    {
      error.friendlyDetails = null;}


    gErrors.set(errorType, error);
    this.notifyStatusChanged();}, 


  clearError: function clearError(errorType) {
    if (gErrors.has(errorType)) {
      gErrors.delete(errorType);
      this.notifyStatusChanged();}}, 



  get errors() {
    return gErrors;}, 


  /**
   * Create a notification channel between the LoopServer and this client
   * via a PushServer. Once created, any subsequent changes in the pushURL
   * assigned by the PushServer will be communicated to the LoopServer.
   * with the Loop server. It will return early if already registered.
   *
   * @param {String} channelID Unique identifier for the notification channel
   *                 registered with the PushServer.
   * @param {LOOP_SESSION_TYPE} sessionType
   * @param {String} serviceType Only 'rooms' is supported.
   * @param {Function} onNotification Callback function that will be associated
   *                   with this channel from the PushServer.
   * @returns {Promise} A promise that is resolved with no params on completion, or
   *                    rejected with an error code or string.
   */
  createNotificationChannel: function createNotificationChannel(channelID, sessionType, serviceType, onNotification) {var _this = this;
    log.debug("createNotificationChannel", channelID, sessionType, serviceType);
    // Wrap the push notification registration callback in a Promise.
    return new Promise(function (resolve, reject) {
      var onRegistered = function onRegistered(error, pushURL, chID) {
        log.debug("createNotificationChannel onRegistered:", error, pushURL, chID);
        if (error) {
          reject(Error(error));} else 
        {
          resolve(_this.registerWithLoopServer(sessionType, serviceType, pushURL));}};



      _this.pushHandler.register(channelID, onRegistered, onNotification);});}, 



  /**
   * Starts registration of Loop with the PushServer and the LoopServer.
   * Successful PushServer registration will automatically trigger the registration
   * of the PushURL returned by the PushServer with the LoopServer. If the registration
   * chain has already been set up, this function will simply resolve.
   *
   * @param {LOOP_SESSION_TYPE} sessionType
   * @returns {Promise} a promise that is resolved with no params on completion, or
   *          rejected with an error code or string.
   */
  promiseRegisteredWithServers: function promiseRegisteredWithServers() {var _this2 = this;var sessionType = arguments.length <= 0 || arguments[0] === undefined ? LOOP_SESSION_TYPE.GUEST : arguments[0];
    if (sessionType !== LOOP_SESSION_TYPE.GUEST && sessionType !== LOOP_SESSION_TYPE.FXA) {
      return Promise.reject(new Error("promiseRegisteredWithServers: Invalid sessionType"));}


    if (this.deferredRegistrations.has(sessionType)) {
      log.debug("promiseRegisteredWithServers: registration already completed or in progress:", 
      sessionType);
      return this.deferredRegistrations.get(sessionType);}


    var options = this.mocks.webSocket ? { mockWebSocket: this.mocks.webSocket } : {};
    this.pushHandler.initialize(options); // This can be called more than once.

    var regPromise = void 0;
    if (sessionType == LOOP_SESSION_TYPE.GUEST) {
      regPromise = this.createNotificationChannel(
      MozLoopService.channelIDs.roomsGuest, sessionType, "rooms", 
      roomsPushNotification);} else 
    {
      regPromise = this.createNotificationChannel(
      MozLoopService.channelIDs.roomsFxA, sessionType, "rooms", 
      roomsPushNotification);}


    log.debug("assigning to deferredRegistrations for sessionType:", sessionType);
    this.deferredRegistrations.set(sessionType, regPromise);

    // Do not return the new Promise generated by this catch() invocation.
    // This will be called along with any other onReject function attached to regPromise.
    regPromise.catch(function (error) {
      log.error("Failed to register with Loop server with sessionType ", sessionType, error);
      _this2.deferredRegistrations.delete(sessionType);
      log.debug("Cleared deferredRegistration for sessionType:", sessionType);});


    return regPromise;}, 


  /**
   * Registers with the Loop server either as a guest or a FxA user.
   *
   * @private
   * @param {LOOP_SESSION_TYPE} sessionType The type of session e.g. guest or FxA
   * @param {String} serviceType: only "rooms" is currently supported.
   * @param {Boolean} [retry=true] Whether to retry if authentication fails.
   * @return {Promise} resolves to pushURL or rejects with an Error
   */
  registerWithLoopServer: function registerWithLoopServer(sessionType, serviceType, pushURL) {var _this3 = this;var retry = arguments.length <= 3 || arguments[3] === undefined ? true : arguments[3];
    log.debug("registerWithLoopServer with sessionType:", sessionType, serviceType, retry);
    if (!pushURL || !sessionType || !serviceType) {
      return Promise.reject(new Error("Invalid or missing parameters for registerWithLoopServer"));}


    var pushURLs = this.pushURLs.get(sessionType);

    // Create a blank URL record set if none exists for this sessionType.
    if (!pushURLs) {
      pushURLs = { rooms: undefined };
      this.pushURLs.set(sessionType, pushURLs);}


    if (pushURLs[serviceType] == pushURL) {
      return Promise.resolve(pushURL);}


    var newURLs = { rooms: pushURLs.rooms };
    newURLs[serviceType] = pushURL;

    return this.hawkRequestInternal(sessionType, "/registration", "POST", 
    { simplePushURLs: newURLs }).then(
    function (response) {
      // If this failed we got an invalid token.
      if (!_this3.storeSessionToken(sessionType, response.headers)) {
        throw new Error("session-token-wrong-size");}


      // Record the new push URL
      pushURLs[serviceType] = pushURL;
      log.debug("Successfully registered with server for sessionType", sessionType);
      _this3.clearError("registration");
      return pushURL;}, 
    function (error) {
      // There's other errors than invalid auth token, but we should only do the reset
      // as a last resort.
      if (error.code === 401) {
        // Authorization failed, invalid token, we need to try again with a new token.
        // XXX (pkerr) - Why is there a retry here? This will not clear up a hawk session
        // token problem at this level.
        if (retry) {
          return _this3.registerWithLoopServer(sessionType, serviceType, pushURL, false);}}



      log.error("Failed to register with the loop server. Error: ", error);
      throw error;});}, 




  /**
   * Unregisters from the Loop server either as a guest or a FxA user.
   *
   * This is normally only wanted for FxA users as we normally want to keep the
   * guest session with the device.
   *
   * NOTE: It is the responsibiliy of the caller the clear the session token
   * after all of the notification classes: rooms, for either
   * Guest or FxA have been unregistered with the LoopServer.
   *
   * @param {LOOP_SESSION_TYPE} sessionType The type of session e.g. guest or FxA
   * @return {Promise} resolving when the unregistration request finishes
   */
  unregisterFromLoopServer: function unregisterFromLoopServer(sessionType) {
    var prefType = Services.prefs.getPrefType(this.getSessionTokenPrefName(sessionType));
    if (prefType == Services.prefs.PREF_INVALID) {
      log.debug("already unregistered from LoopServer", sessionType);
      return Promise.resolve("already unregistered");}


    var error = void 0, 
    pushURLs = this.pushURLs.get(sessionType), 
    roomsPushURL = pushURLs ? pushURLs.rooms : null;
    this.pushURLs.delete(sessionType);

    if (!roomsPushURL) {
      return Promise.resolve("no pushURL of this type to unregister");}


    var unregisterURL = "/registration?simplePushURL=" + encodeURIComponent(roomsPushURL);
    return this.hawkRequestInternal(sessionType, unregisterURL, "DELETE").then(
    function () {
      log.debug("Successfully unregistered from server for sessionType = ", sessionType);
      return "unregistered sessionType " + sessionType;}, 

    function (err) {
      if (err.code === 401) {
        // Authorization failed, invalid token. This is fine since it may mean we already logged out.
        log.debug("already unregistered - invalid token", sessionType);
        return "already unregistered, sessionType = " + sessionType;}


      log.error("Failed to unregister with the loop server. Error: ", error);
      throw err;});}, 



  /**
   * Performs a hawk based request to the loop server - there is no pre-registration
   * for this request, if this is required, use hawkRequest.
   *
   * @param {LOOP_SESSION_TYPE} sessionType The type of session to use for the request.
   *                                        This is one of the LOOP_SESSION_TYPE members.
   * @param {String} path The path to make the request to.
   * @param {String} method The request method, e.g. 'POST', 'GET'.
   * @param {Object} payloadObj An object which is converted to JSON and
   *                            transmitted with the request.
   * @param {Boolean} [retryOn401=true] Whether to retry if authentication fails.
   * @returns {Promise}
   *        Returns a promise that resolves to the response of the API call,
   *        or is rejected with an error.  If the server response can be parsed
   *        as JSON and contains an 'error' property, the promise will be
   *        rejected with this JSON-parsed response.
   */
  hawkRequestInternal: function hawkRequestInternal(sessionType, path, method, payloadObj) {var _this4 = this;var retryOn401 = arguments.length <= 4 || arguments[4] === undefined ? true : arguments[4];
    log.debug("hawkRequestInternal: ", sessionType, path, method);
    if (!gHawkClient) {
      gHawkClient = new HawkClient(this.loopServerUri);}


    var sessionToken = void 0, credentials = void 0;
    try {
      sessionToken = Services.prefs.getCharPref(this.getSessionTokenPrefName(sessionType));} 
    catch (x) {
      // It is ok for this not to exist, we'll default to sending no-creds
    }

    if (sessionToken) {
      // true = use a hex key, as required by the server (see bug 1032738).
      credentials = deriveHawkCredentials(sessionToken, "sessionToken", 
      2 * 32, true);}


    // Later versions of Firefox will do utf-8 encoding of the request, but
    // we need to do it ourselves for older versions.
    if (!gHawkClient.willUTF8EncodeRequests && payloadObj) {
      // Note: we must copy the object rather than mutate it, to avoid
      // mutating the values of the object passed in.
      var newPayloadObj = {};var _iteratorNormalCompletion = true;var _didIteratorError = false;var _iteratorError = undefined;try {
        for (var _iterator = Object.getOwnPropertyNames(payloadObj)[Symbol.iterator](), _step; !(_iteratorNormalCompletion = (_step = _iterator.next()).done); _iteratorNormalCompletion = true) {var property = _step.value;
          if (typeof payloadObj[property] == "string") {
            newPayloadObj[property] = CommonUtils.encodeUTF8(payloadObj[property]);} else 
          {
            newPayloadObj[property] = payloadObj[property];}}} catch (err) {_didIteratorError = true;_iteratorError = err;} finally {try {if (!_iteratorNormalCompletion && _iterator.return) {_iterator.return();}} finally {if (_didIteratorError) {throw _iteratorError;}}}


      payloadObj = newPayloadObj;}


    var handle401Error = function handle401Error(error) {
      if (sessionType === LOOP_SESSION_TYPE.FXA) {
        return MozLoopService.logOutFromFxA().then(function () {
          // Set a user-visible error after logOutFromFxA clears existing ones.
          _this4.setError("login", error);
          throw error;});}


      _this4.setError("registration", error);
      throw error;};


    var extraHeaders = { 
      "x-loop-addon-ver": gAddonVersion };


    return gHawkClient.request(path, method, credentials, payloadObj, extraHeaders).then(
    function (result) {
      _this4.clearError("network");
      return result;}, 

    function (error) {
      if (error.code && error.code == 401) {
        _this4.clearSessionToken(sessionType);
        if (retryOn401 && sessionType === LOOP_SESSION_TYPE.GUEST) {
          log.info("401 and INVALID_AUTH_TOKEN - retry registration");
          return _this4.registerWithLoopServer(sessionType, false).then(
          function () {return _this4.hawkRequestInternal(sessionType, path, method, payloadObj, false);}, 
          // Process the original error that triggered the retry.
          function () {return handle401Error(error);});}


        return handle401Error(error);}

      throw error;});}, 



  /**
   * Performs a hawk based request to the loop server, registering if necessary.
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
  hawkRequest: function hawkRequest(sessionType, path, method, payloadObj) {var _this5 = this;
    log.debug("hawkRequest: " + path, sessionType);
    return new Promise(function (resolve, reject) {
      MozLoopService.promiseRegisteredWithServers(sessionType).then(function () {
        _this5.hawkRequestInternal(sessionType, path, method, payloadObj).then(resolve, reject);}, 
      function (err) {
        reject(err);}).
      catch(reject);});}, 



  /**
   * Generic hawkRequest onError handler for the hawkRequest promise.
   *
   * @param {Object} error - error reporting object
   *
   */

  _hawkRequestError: function _hawkRequestError(error) {
    log.error("Loop hawkRequest error:", error);
    throw error;}, 


  getSessionTokenPrefName: function getSessionTokenPrefName(sessionType) {
    var suffix = void 0;
    switch (sessionType) {
      case LOOP_SESSION_TYPE.GUEST:
        suffix = "";
        break;
      case LOOP_SESSION_TYPE.FXA:
        suffix = ".fxa";
        break;
      default:
        throw new Error("Unknown LOOP_SESSION_TYPE");}

    return "loop.hawk-session-token" + suffix;}, 


  /**
   * Used to store a session token from a request if it exists in the headers.
   *
   * @param {LOOP_SESSION_TYPE} sessionType The type of session to use for the request.
   *                                        One of the LOOP_SESSION_TYPE members.
   * @param {Object} headers The request headers, which may include a
   *                         "hawk-session-token" to be saved.
   * @return true on success or no token, false on failure.
   */
  storeSessionToken: function storeSessionToken(sessionType, headers) {
    var sessionToken = headers["hawk-session-token"];
    if (sessionToken) {
      // XXX should do more validation here
      if (sessionToken.length === 64) {
        Services.prefs.setCharPref(this.getSessionTokenPrefName(sessionType), sessionToken);
        log.debug("Stored a hawk session token for sessionType", sessionType);} else 
      {
        // XXX Bubble the precise details up to the UI somehow (bug 1013248).
        log.warn("Loop server sent an invalid session token");
        return false;}}


    return true;}, 


  /**
   * Clear the loop session token so we don't use it for Hawk Requests anymore.
   *
   * This should normally be used after unregistering with the server so it can
   * clean up session state first.
   *
   * @param {LOOP_SESSION_TYPE} sessionType The type of session to use for the request.
   *                                        One of the LOOP_SESSION_TYPE members.
   */
  clearSessionToken: function clearSessionToken(sessionType) {
    Services.prefs.clearUserPref(this.getSessionTokenPrefName(sessionType));
    log.debug("Cleared hawk session token for sessionType", sessionType);}, 


  /**
   * A getter to obtain and store the strings for loop. This is structured
   * for use by l10n.js.
   *
   * @returns {Map} a map of element ids with localized string values
   */
  get localizedStrings() {
    if (gLocalizedStrings.size) {
      return gLocalizedStrings;}


    // Load all strings from a bundle location preferring strings loaded later.
    function loadAllStrings(location) {
      var bundle = Services.strings.createBundle(location);
      var enumerator = bundle.getSimpleEnumeration();
      while (enumerator.hasMoreElements()) {
        var string = enumerator.getNext().QueryInterface(Ci.nsIPropertyElement);
        gLocalizedStrings.set(string.key, string.value);}}



    // Load fallback/en-US strings then prefer the localized ones if available.
    loadAllStrings("chrome://loop-locale-fallback/content/loop.properties");
    loadAllStrings("chrome://loop/locale/loop.properties");

    // Supply the strings from the branding bundle on a per-need basis.
    var brandBundle = 
    Services.strings.createBundle("chrome://branding/locale/brand.properties");
    // Unfortunately the `brandShortName` string is used by Loop with a lowercase 'N'.
    gLocalizedStrings.set("brandShortname", brandBundle.GetStringFromName("brandShortName"));

    return gLocalizedStrings;}, 


  /**
   * Saves loop logs to the saved-telemetry-pings folder.
   *
   * @param {nsIDOMWindow} window The window object which can be communicated with
   * @param {Object}        The peerConnection in question.
   */
  stageForTelemetryUpload: function stageForTelemetryUpload(window, details) {
    var mm = window.messageManager;
    mm.addMessageListener("Loop:GetAllWebrtcStats", function getAllStats(message) {
      mm.removeMessageListener("Loop:GetAllWebrtcStats", getAllStats);var _message$data = 

      message.data;var allStats = _message$data.allStats;var logs = _message$data.logs;
      var internalFormat = allStats.reports[0]; // filtered on peerConnectionID

      var report = convertToRTCStatsReport(internalFormat);
      var logStr = "";
      logs.forEach(function (s) {
        logStr += s + "\n";});


      // We have stats and logs.

      // Create worker job. ping = saved telemetry ping file header + payload
      //
      // Prepare payload according to https://wiki.mozilla.org/Loop/Telemetry

      var ai = Services.appinfo;
      var uuid = uuidgen.generateUUID().toString();
      uuid = uuid.substr(1, uuid.length - 2); // remove uuid curly braces

      var directory = OS.Path.join(OS.Constants.Path.profileDir, 
      "saved-telemetry-pings");
      var job = { 
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
              version: Services.sysinfo.getProperty("version") }, 

            report: "ice failure", 
            connectionstate: details.iceConnectionState, 
            stats: report, 
            localSdp: internalFormat.localSdp, 
            remoteSdp: internalFormat.remoteSdp, 
            log: logStr } } };




      // Send job to worker to do log sanitation, transcoding and saving to
      // disk for pickup by telemetry on next startup, which then uploads it.

      var worker = new ChromeWorker("MozLoopWorker.js");
      worker.onmessage = function (e) {
        log.info(e.data.ok ? 
        "Successfully staged loop report for telemetry upload." : 
        "Failed to stage loop report. Error: " + e.data.fail);};

      worker.postMessage(job);});


    mm.sendAsyncMessage("Loop:GetAllWebrtcStats", { 
      peerConnectionID: details.peerConnectionID });}, 



  /**
   * Gets an id for the chat window, for now we just use the roomToken.
   *
   * @param  {Object} conversationWindowData The conversation window data.
   */
  getChatWindowID: function getChatWindowID(conversationWindowData) {
    return conversationWindowData.roomToken;}, 


  getChatURL: function getChatURL(chatWindowId) {
    return "about:loopconversation#" + chatWindowId;}, 


  getChatWindows: function getChatWindows() {
    var isLoopURL = function isLoopURL(_ref) {var src = _ref.src;return (/^about:loopconversation#/.test(src));};
    return [].concat(_toConsumableArray(Chat.chatboxes)).filter(isLoopURL);}, 


  /**
   * Hangup and close all chat windows that are open.
   */
  hangupAllChatWindows: function hangupAllChatWindows() {var _iteratorNormalCompletion2 = true;var _didIteratorError2 = false;var _iteratorError2 = undefined;try {
      for (var _iterator2 = this.getChatWindows()[Symbol.iterator](), _step2; !(_iteratorNormalCompletion2 = (_step2 = _iterator2.next()).done); _iteratorNormalCompletion2 = true) {var chatbox = _step2.value;
        var mm = chatbox.content.messageManager;
        mm.sendAsyncMessage("Social:CustomEvent", { name: "LoopHangupNow" });}} catch (err) {_didIteratorError2 = true;_iteratorError2 = err;} finally {try {if (!_iteratorNormalCompletion2 && _iterator2.return) {_iterator2.return();}} finally {if (_didIteratorError2) {throw _iteratorError2;}}}}, 



  /**
   * Pause or resume all chat windows that are open.
   */
  toggleBrowserSharing: function toggleBrowserSharing() {var on = arguments.length <= 0 || arguments[0] === undefined ? true : arguments[0];var _iteratorNormalCompletion3 = true;var _didIteratorError3 = false;var _iteratorError3 = undefined;try {
      for (var _iterator3 = this.getChatWindows()[Symbol.iterator](), _step3; !(_iteratorNormalCompletion3 = (_step3 = _iterator3.next()).done); _iteratorNormalCompletion3 = true) {var chatbox = _step3.value;
        var mm = chatbox.content.messageManager;
        mm.sendAsyncMessage("Social:CustomEvent", { 
          name: "ToggleBrowserSharing", 
          detail: on });}} catch (err) {_didIteratorError3 = true;_iteratorError3 = err;} finally {try {if (!_iteratorNormalCompletion3 && _iterator3.return) {_iterator3.return();}} finally {if (_didIteratorError3) {throw _iteratorError3;}}}}, 




  /**
   * Determines if a chat window is already open for a given window id.
   *
   * @param  {String}  chatWindowId The window id.
   * @return {Boolean}              True if the window is opened.
   */
  isChatWindowOpen: function isChatWindowOpen(chatWindowId) {
    if (this.mocks.isChatWindowOpen !== undefined) {
      return this.mocks.isChatWindowOpen;}


    var chatUrl = this.getChatURL(chatWindowId);

    return [].concat(_toConsumableArray(Chat.chatboxes)).some(function (chatbox) {return chatbox.src == chatUrl;});}, 


  /**
   * Opens the chat window
   *
   * @param {Object} conversationWindowData The data to be obtained by the
   *                                        window when it opens.
   * @param {Function} windowCloseCallback  Callback function that's invoked
   *                                        when the window closes.
   * @returns {Promise} That is resolved with the id of the window, null if a
   *                    window could not be opened.
   */
  openChatWindow: function openChatWindow(conversationWindowData, windowCloseCallback) {var _this6 = this;
    return new Promise(function (resolve) {
      // So I guess the origin is the loop server!?
      var origin = _this6.loopServerUri;
      var windowId = _this6.getChatWindowID(conversationWindowData);

      gConversationWindowData.set(windowId, conversationWindowData);

      var url = _this6.getChatURL(windowId);

      // Ensure about:loopconversation has access to the camera.
      Services.perms.add(Services.io.newURI(url, null, null), "camera", 
      Services.perms.ALLOW_ACTION, Services.perms.EXPIRE_SESSION);

      Chat.registerButton(kChatboxHangupButton);

      var callback = function callback(chatbox) {
        var mm = chatbox.content.messageManager;

        var loaded = function loaded() {
          mm.removeMessageListener("DOMContentLoaded", loaded);
          mm.sendAsyncMessage("Social:ListenForEvents", { 
            eventNames: ["LoopChatEnabled", "LoopChatMessageAppended", 
            "LoopChatDisabledMessageAppended", "socialFrameAttached", 
            "socialFrameDetached", "socialFrameHide", "socialFrameShow"] });


          var kEventNamesMap = { 
            socialFrameAttached: "Loop:ChatWindowAttached", 
            socialFrameDetached: "Loop:ChatWindowDetached", 
            socialFrameHide: "Loop:ChatWindowHidden", 
            socialFrameShow: "Loop:ChatWindowShown", 
            unload: "Loop:ChatWindowClosed" };


          var kSizeMap = { 
            LoopChatEnabled: "loopChatEnabled", 
            LoopChatDisabledMessageAppended: "loopChatDisabledMessageAppended", 
            LoopChatMessageAppended: "loopChatMessageAppended" };


          var listeners = {};

          var messageName = "Social:CustomEvent";
          mm.addMessageListener(messageName, listeners[messageName] = function (message) {
            var eventName = message.data.name;
            if (kEventNamesMap[eventName]) {
              eventName = kEventNamesMap[eventName];

              // `clearAvailableTargetsCache` is new in Firefox 46. The else branch
              // supports Firefox 45.
              if ("clearAvailableTargetsCache" in UITour) {
                UITour.clearAvailableTargetsCache();} else 
              {
                UITour.availableTargetsCache.clear();}

              UITour.notify(eventName);} else 
            {
              // When the chat box or messages are shown, resize the panel or window
              // to be slightly higher to accomodate them.
              var customSize = kSizeMap[eventName];
              var currSize = chatbox.getAttribute("customSize");
              // If the size is already at the requested one or at the maximum size
              // already, don't do anything. Especially don't make it shrink.
              if (customSize && currSize != customSize && currSize != "loopChatMessageAppended") {
                chatbox.setAttribute("customSize", customSize);
                chatbox.parentNode.setAttribute("customSize", customSize);}}});




          // Disable drag feature if needed
          if (!MozLoopService.getLoopPref("conversationPopOut.enabled")) {
            var document = chatbox.ownerDocument;
            var titlebarNode = document.getAnonymousElementByAttribute(chatbox, "class", 
            "chat-titlebar");
            titlebarNode.addEventListener("dragend", function (event) {
              event.stopPropagation();
              return false;});}



          // Handle window.close correctly on the chatbox.
          mm.sendAsyncMessage("Social:HookWindowCloseForPanelClose");
          messageName = "Social:DOMWindowClose";
          mm.addMessageListener(messageName, listeners[messageName] = function () {
            chatbox.close();});


          mm.sendAsyncMessage("Loop:MonitorPeerConnectionLifecycle");
          messageName = "Loop:PeerConnectionLifecycleChange";
          mm.addMessageListener(messageName, listeners[messageName] = function (message) {
            // Chat Window Id, this is different that the internal winId
            var chatWindowId = message.data.locationHash.slice(1);
            var context = _this6.conversationContexts.get(chatWindowId);
            var peerConnectionID = message.data.peerConnectionID;
            var exists = peerConnectionID.match(/session=(\S+)/);
            if (context && !exists) {
              // Not ideal but insert our data amidst existing data like this:
              // - 000 (id=00 url=http)
              // + 000 (session=000 call=000 id=00 url=http)
              var pair = peerConnectionID.split("(");
              if (pair.length == 2) {
                peerConnectionID = pair[0] + "(session=" + context.sessionId + (
                context.callId ? " call=" + context.callId : "") + " " + pair[1];}}



            if (message.data.type == "iceconnectionstatechange") {
              switch (message.data.iceConnectionState) {
                case "failed":
                case "disconnected":
                  if (Services.telemetry.canRecordExtended) {
                    _this6.stageForTelemetryUpload(chatbox.content, message.data);}

                  break;}}});




          var closeListener = function closeListener() {
            this.removeEventListener("ChatboxClosed", closeListener);

            // Remove message listeners.
            var _iteratorNormalCompletion4 = true;var _didIteratorError4 = false;var _iteratorError4 = undefined;try {for (var _iterator4 = Object.getOwnPropertyNames(listeners)[Symbol.iterator](), _step4; !(_iteratorNormalCompletion4 = (_step4 = _iterator4.next()).done); _iteratorNormalCompletion4 = true) {var name = _step4.value;
                mm.removeMessageListener(name, listeners[name]);}} catch (err) {_didIteratorError4 = true;_iteratorError4 = err;} finally {try {if (!_iteratorNormalCompletion4 && _iterator4.return) {_iterator4.return();}} finally {if (_didIteratorError4) {throw _iteratorError4;}}}

            listeners = {};

            windowCloseCallback();

            if (conversationWindowData.type == "room") {
              // NOTE: if you add something here, please also consider if something
              //       needs to be done on the content side as well (e.g.
              //       activeRoomStore#windowUnload).
              LoopAPI.sendMessageToHandler({ 
                name: "HangupNow", 
                data: [conversationWindowData.roomToken, windowId] });}};




          // When a chat window is attached or detached, the docShells hosting
          // about:loopconverstation is swapped to the newly created chat window.
          // (Be it inside a popup or back inside a chatbox element attached to the
          // chatbar.)
          // Since a swapDocShells call does not swap the messageManager instances
          // attached to a browser, we'll need to add the message listeners to
          // the new messageManager. This is not a bug in swapDocShells, merely
          // a design decision.
          chatbox.content.addEventListener("SwapDocShells", function swapped(ev) {
            this.removeEventListener("SwapDocShells", swapped);
            this.removeEventListener("ChatboxClosed", closeListener);

            var otherBrowser = ev.detail;
            chatbox = otherBrowser.ownerDocument.getBindingParent(otherBrowser);
            mm = otherBrowser.messageManager;
            otherBrowser.addEventListener("SwapDocShells", swapped);
            chatbox.addEventListener("ChatboxClosed", closeListener);var _iteratorNormalCompletion5 = true;var _didIteratorError5 = false;var _iteratorError5 = undefined;try {

              for (var _iterator5 = Object.getOwnPropertyNames(listeners)[Symbol.iterator](), _step5; !(_iteratorNormalCompletion5 = (_step5 = _iterator5.next()).done); _iteratorNormalCompletion5 = true) {var name = _step5.value;
                mm.addMessageListener(name, listeners[name]);}} catch (err) {_didIteratorError5 = true;_iteratorError5 = err;} finally {try {if (!_iteratorNormalCompletion5 && _iterator5.return) {_iterator5.return();}} finally {if (_didIteratorError5) {throw _iteratorError5;}}}});



          chatbox.addEventListener("ChatboxClosed", closeListener);

          UITour.notify("Loop:ChatWindowOpened");
          resolve(windowId);};


        mm.sendAsyncMessage("WaitForDOMContentLoaded");
        mm.addMessageListener("DOMContentLoaded", loaded);};


      LoopAPI.initialize();
      var chatboxInstance = Chat.open(null, { 
        origin: origin, 
        title: "", 
        url: url, 
        remote: MozLoopService.getLoopPref("remote.autostart") }, 
      callback);
      if (!chatboxInstance) {
        resolve(null);
        // It's common for unit tests to overload Chat.open, so check if we actually
        // got a DOM node back.
      } else if (chatboxInstance.setAttribute) {
        // Set properties that influence visual appearance of the chatbox right
        // away to circumvent glitches.
        chatboxInstance.setAttribute("customSize", "loopDefault");
        chatboxInstance.parentNode.setAttribute("customSize", "loopDefault");
        var buttons = "minimize,";
        if (MozLoopService.getLoopPref("conversationPopOut.enabled")) {
          buttons += "swap,";}

        Chat.loadButtonSet(chatboxInstance, buttons + kChatboxHangupButton.id);
        // Final fall-through in case a unit test overloaded Chat.open. Here we can
        // immediately resolve the promise.
      } else {
        resolve(windowId);}});}, 




  /**
   * Fetch Firefox Accounts (FxA) OAuth parameters from the Loop Server.
   *
   * @return {Promise} resolved with the body of the hawk request for OAuth parameters.
   */
  promiseFxAOAuthParameters: function promiseFxAOAuthParameters() {var _this7 = this;
    var SESSION_TYPE = LOOP_SESSION_TYPE.FXA;
    return this.hawkRequestInternal(SESSION_TYPE, "/fxa-oauth/params", "POST").then(function (response) {
      if (!_this7.storeSessionToken(SESSION_TYPE, response.headers)) {
        throw new Error("Invalid FxA hawk token returned");}

      var prefType = Services.prefs.getPrefType(_this7.getSessionTokenPrefName(SESSION_TYPE));
      if (prefType == Services.prefs.PREF_INVALID) {
        throw new Error("No FxA hawk token returned and we don't have one saved");}


      return JSON.parse(response.body);}, 

    function (error) {_this7._hawkRequestError(error);});}, 


  /**
   * Get the OAuth client constructed with Loop OAauth parameters.
   *
   * @param {Boolean} forceReAuth Set to true to force the user to reauthenticate.
   * @return {Promise}
   */
  promiseFxAOAuthClient: Task.async(function* (forceReAuth) {
    // We must make sure to have only a single client otherwise they will have different states and
    // multiple channels. This would happen if the user clicks the Login button more than once.
    if (gFxAOAuthClientPromise) {
      return gFxAOAuthClientPromise;}


    gFxAOAuthClientPromise = this.promiseFxAOAuthParameters().then(
    function (parameters) {
      // Add the fact that we want keys to the parameters.
      parameters.keys = true;
      if (forceReAuth) {
        parameters.action = "force_auth";
        parameters.email = MozLoopService.userProfile.email;}


      try {
        gFxAOAuthClient = new FxAccountsOAuthClient({ 
          parameters: parameters });} 

      catch (ex) {
        gFxAOAuthClientPromise = null;
        throw ex;}

      return gFxAOAuthClient;}, 

    function (error) {
      gFxAOAuthClientPromise = null;
      throw error;});



    return gFxAOAuthClientPromise;}), 


  /**
   * Get the OAuth client and do the authorization web flow to get an OAuth code.
   *
   * @param {Boolean} forceReAuth Set to true to force the user to reauthenticate.
   * @return {Promise}
   */
  promiseFxAOAuthAuthorization: function promiseFxAOAuthAuthorization(forceReAuth) {var _this8 = this;
    var deferred = Promise.defer();
    this.promiseFxAOAuthClient(forceReAuth).then(
    function (client) {
      client.onComplete = _this8._fxAOAuthComplete.bind(_this8, deferred);
      client.onError = _this8._fxAOAuthError.bind(_this8, deferred);
      client.launchWebFlow();}, 

    function (error) {
      log.error(error);
      deferred.reject(error);});


    return deferred.promise;}, 


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
  promiseFxAOAuthToken: function promiseFxAOAuthToken(code, state) {var _this9 = this;
    if (!code || !state) {
      throw new Error("promiseFxAOAuthToken: code and state are required.");}


    var payload = { 
      code: code, 
      state: state };

    return this.hawkRequestInternal(LOOP_SESSION_TYPE.FXA, "/fxa-oauth/token", "POST", payload).then(
    function (response) {return JSON.parse(response.body);}, function (error) {return _this9._hawkRequestError(error);});}, 


  /**
   * Called once gFxAOAuthClient fires onComplete.
   *
   * @param {Deferred} deferred used to resolve the gFxAOAuthClientPromise
   * @param {Object} result (with code and state)
   */
  _fxAOAuthComplete: function _fxAOAuthComplete(deferred, result, keys) {
    if (keys.kBr) {
      // Trim Base64 padding from relier keys.
      Services.prefs.setCharPref("loop.key.fxa", keys.kBr.k.replace(/=/g, "")); // eslint-disable-line no-div-regex
    }
    gFxAOAuthClientPromise = null;
    // Note: The state was already verified in FxAccountsOAuthClient.
    deferred.resolve(result);}, 


  /**
   * Called if gFxAOAuthClient fires onError.
   *
   * @param {Deferred} deferred used to reject the gFxAOAuthClientPromise
   * @param {Object} error object returned by FxAOAuthClient
   */
  _fxAOAuthError: function _fxAOAuthError(deferred, err) {
    gFxAOAuthClientPromise = null;
    deferred.reject(err);} };


Object.freeze(MozLoopServiceInternal);


var gInitializeTimerFunc = function gInitializeTimerFunc(deferredInitialization) {
  // Kick off the push notification service into registering after a timeout.
  // This ensures we're not doing too much straight after the browser's finished
  // starting up.

  setTimeout(MozLoopService.delayedInitialize.bind(MozLoopService, deferredInitialization), 
  MozLoopServiceInternal.initialRegistrationDelayMilliseconds);};


var gServiceInitialized = false;

/**
 * Public API
 */
this.MozLoopService = { 
  _DNSService: gDNSService, 
  _activeScreenShares: new Set(), 

  get channelIDs() {
    // Channel ids that will be registered with the PushServer for notifications
    return { 
      roomsFxA: "6add272a-d316-477c-8335-f00f73dfde71", 
      roomsGuest: "19d3f799-a8f3-4328-9822-b7cd02765832" };}, 



  /**
   * Used to override the initalize timer function for test purposes.
   */
  set initializeTimerFunc(value) {
    gInitializeTimerFunc = value;}, 


  /**
   * Used to reset if the service has been initialized or not - for test
   * purposes.
   */
  resetServiceInitialized: function resetServiceInitialized() {
    gServiceInitialized = false;}, 


  get roomsParticipantsCount() {
    return LoopRooms.participantsCount;}, 


  /**
   * Initialized the loop service, and starts registration with the
   * push and loop servers.
   *
   * Note: this returns a promise for unit test purposes.
   *
   * @param {String} addonVersion The name of the add-on
   *
   * @return {Promise}
   */
  initialize: Task.async(function* (addonVersion) {
    // Ensure we don't setup things like listeners more than once.
    if (gServiceInitialized) {
      return Promise.resolve();}


    gAddonVersion = addonVersion;

    gServiceInitialized = true;

    // Do this here, rather than immediately after definition, so that we can
    // stub out API functions for unit testing
    Object.freeze(this);

    // Initialise anything that needs it in rooms.
    LoopRooms.init();

    // Don't do anything if loop is not enabled.
    if (!Services.prefs.getBoolPref("loop.enabled")) {
      return Promise.reject(new Error("loop is not enabled"));}


    // The Loop toolbar button should change icon when the room participant count
    // changes from 0 to something.
    var onRoomsChange = function onRoomsChange(e) {
      // Pass the event name as notification reason for better logging.
      MozLoopServiceInternal.notifyStatusChanged("room-" + e);};

    LoopRooms.on("add", onRoomsChange);
    LoopRooms.on("update", onRoomsChange);
    LoopRooms.on("delete", onRoomsChange);
    LoopRooms.on("joined", function (e, room, participant) {
      // Don't alert if we're in the doNotDisturb mode, or the participant
      // is the owner - the content code deals with the rest of the sounds.
      if (MozLoopServiceInternal.doNotDisturb || participant.owner) {
        return;}


      var window = gWM.getMostRecentWindow("navigator:browser");
      if (window) {
        // The participant that joined isn't necessarily included in room.participants (depending on
        // when the broadcast happens) so concatenate.
        var isOwnerInRoom = room.participants.concat(participant).some(function (p) {return p.owner;});
        var bundle = MozLoopServiceInternal.localizedStrings;

        var localizedString = void 0;
        if (isOwnerInRoom) {
          localizedString = bundle.get("rooms_room_joined_owner_connected_label2");} else 
        {
          var l10nString = bundle.get("rooms_room_joined_owner_not_connected_label");
          var roomUrlHostname = new URL(room.decryptedContext.urls[0].location).hostname.replace(/^www\./, "");
          localizedString = l10nString.replace("{{roomURLHostname}}", roomUrlHostname);}

        window.LoopUI.showNotification({ 
          sound: "room-joined", 
          // Fallback to the brand short name if the roomName isn't available.
          title: room.roomName || MozLoopServiceInternal.localizedStrings.get("clientShortname2"), 
          message: localizedString, 
          selectTab: "rooms" });}});




    LoopRooms.on("joined", this.maybeResumeTourOnRoomJoined.bind(this));

    // If there's no guest room created and the user hasn't
    // previously authenticated then skip registration.
    if (!LoopRooms.getGuestCreatedRoom() && 
    !MozLoopServiceInternal.fxAOAuthTokenData) {
      return Promise.resolve("registration not needed");}


    var deferredInitialization = Promise.defer();
    gInitializeTimerFunc(deferredInitialization);

    return deferredInitialization.promise;}), 


  /**
   * Maybe resume the tour (re-opening the tab, if necessary) if someone else joins
   * a room of ours and it's currently open.
   */
  maybeResumeTourOnRoomJoined: function maybeResumeTourOnRoomJoined(e, room, participant) {
    var isOwnerInRoom = false;
    var isOtherInRoom = false;

    if (!this.getLoopPref("gettingStarted.resumeOnFirstJoin")) {
      return;}


    if (!room.participants) {
      return;}


    // The participant that joined isn't necessarily included in room.participants (depending on
    // when the broadcast happens) so concatenate.
    var _iteratorNormalCompletion6 = true;var _didIteratorError6 = false;var _iteratorError6 = undefined;try {for (var _iterator6 = room.participants.concat(participant)[Symbol.iterator](), _step6; !(_iteratorNormalCompletion6 = (_step6 = _iterator6.next()).done); _iteratorNormalCompletion6 = true) {var roomParticipant = _step6.value;
        if (roomParticipant.owner) {
          isOwnerInRoom = true;} else 
        {
          isOtherInRoom = true;}}} catch (err) {_didIteratorError6 = true;_iteratorError6 = err;} finally {try {if (!_iteratorNormalCompletion6 && _iterator6.return) {_iterator6.return();}} finally {if (_didIteratorError6) {throw _iteratorError6;}}}



    if (!isOwnerInRoom || !isOtherInRoom) {
      return;}


    // Check that the room chatbox is still actually open using its URL
    var chatboxesForRoom = [].concat(_toConsumableArray(Chat.chatboxes)).filter(function (chatbox) {return (
        chatbox.src == MozLoopServiceInternal.getChatURL(room.roomToken));});

    if (!chatboxesForRoom.length) {
      log.warn("Tried to resume the tour from a join when the chatbox was closed", room);
      return;}


    this.resumeTour("open");}, 


  /**
   * The core of the initialization work that happens once the browser is ready
   * (after a timer when called during startup).
   *
   * Can be called more than once (e.g. if the initial setup fails at some phase).
   * @param {Deferred} deferredInitialization
   */
  delayedInitialize: Task.async(function* (deferredInitialization) {
    log.debug("delayedInitialize");
    // Set or clear an error depending on how deferredInitialization gets resolved.
    // We do this first so that it can handle the early returns below.
    var completedPromise = deferredInitialization.promise.then(function (result) {
      MozLoopServiceInternal.clearError("initialization");
      return result;}, 

    function (error) {
      // If we get a non-object then setError was already called for a different error type.
      if ((typeof error === "undefined" ? "undefined" : _typeof(error)) == "object") {
        MozLoopServiceInternal.setError("initialization", error, function () {return MozLoopService.delayedInitialize(Promise.defer());});}});



    try {
      if (LoopRooms.getGuestCreatedRoom()) {
        yield this.promiseRegisteredWithServers(LOOP_SESSION_TYPE.GUEST);} else 
      {
        log.debug("delayedInitialize: Guest Room hasn't been created so not registering as a guest");}} 

    catch (ex) {
      log.debug("MozLoopService: Failure of guest registration", ex);
      deferredInitialization.reject(ex);
      yield completedPromise;
      return;}


    if (!MozLoopServiceInternal.fxAOAuthTokenData) {
      log.debug("delayedInitialize: Initialized without an already logged-in account");
      deferredInitialization.resolve("initialized without FxA status");
      yield completedPromise;
      return;}


    log.debug("MozLoopService: Initializing with already logged-in account");
    MozLoopServiceInternal.promiseRegisteredWithServers(LOOP_SESSION_TYPE.FXA).then(function () {
      deferredInitialization.resolve("initialized to logged-in status");}, 
    function (error) {
      log.debug("MozLoopService: error logging in using cached auth token");
      var retryFunc = function retryFunc() {return MozLoopServiceInternal.promiseRegisteredWithServers(LOOP_SESSION_TYPE.FXA);};
      MozLoopServiceInternal.setError("login", error, retryFunc);
      deferredInitialization.reject("error logging in using cached auth token");});

    yield completedPromise;}), 


  /**
   * Hangup and close all chat windows that are open.
   */
  hangupAllChatWindows: function hangupAllChatWindows() {
    return MozLoopServiceInternal.hangupAllChatWindows();}, 


  toggleBrowserSharing: function toggleBrowserSharing(on) {
    return MozLoopServiceInternal.toggleBrowserSharing(on);}, 


  /**
   * Opens the chat window
   *
   * @param {Object} conversationWindowData The data to be obtained by the
   *                                        window when it opens.
   * @param {Function} windowCloseCallback Callback for when the window closes.
   * @returns {Number} The id of the window.
   */
  openChatWindow: function openChatWindow(conversationWindowData, windowCloseCallback) {
    return MozLoopServiceInternal.openChatWindow(conversationWindowData, 
    windowCloseCallback);}, 


  /**
   * Determines if a chat window is already open for a given window id.
   *
   * @param  {String}  chatWindowId The window id.
   * @return {Boolean}              True if the window is opened.
   */
  isChatWindowOpen: function isChatWindowOpen(chatWindowId) {
    return MozLoopServiceInternal.isChatWindowOpen(chatWindowId);}, 


  /**
   * @see MozLoopServiceInternal.promiseRegisteredWithServers
   */
  promiseRegisteredWithServers: function promiseRegisteredWithServers() {var sessionType = arguments.length <= 0 || arguments[0] === undefined ? LOOP_SESSION_TYPE.GUEST : arguments[0];
    return MozLoopServiceInternal.promiseRegisteredWithServers(sessionType);}, 


  /**
   * Returns the strings for the specified element. Designed for use with l10n.js.
   *
   * @param {key} The element id to get strings for. Optional, returns the whole
   *              map of strings when omitted.
   * @return {String} A JSON string containing the localized attribute/value pairs
   *                  for the element.
   */
  getStrings: function getStrings(key) {
    var stringData = MozLoopServiceInternal.localizedStrings;
    if (!key) {
      return stringData;}

    if (!stringData.has(key)) {
      log.error("No string found for key: ", key);
      return "";}


    return JSON.stringify({ textContent: stringData.get(key) });}, 


  /**
   * Returns the addon version
   *
   * @return {String} A string containing the Addon Version
   */
  get addonVersion() {
    // remove "alpha", "beta" or any non numeric appended to the version string
    var numericAddonVersion = gAddonVersion.replace(/[^0-9\.]/g, "");
    return numericAddonVersion;}, 


  /**
   *
   * Returns a new GUID (UUID) in curly braces format.
   */
  generateUUID: function generateUUID() {
    return uuidgen.generateUUID().toString();}, 


  /**
   * Retrieves MozLoopService "do not disturb" value.
   *
   * @return {Boolean}
   */
  get doNotDisturb() {
    return MozLoopServiceInternal.doNotDisturb;}, 


  /**
   * Sets MozLoopService "do not disturb" value.
   *
   * @param {Boolean} aFlag
   */
  set doNotDisturb(aFlag) {
    MozLoopServiceInternal.doNotDisturb = aFlag;}, 


  /**
   * Gets the user profile, but only if there is
   * tokenData present. Without tokenData, the
   * profile is meaningless.
   *
   * @return {Object}
   */
  get userProfile() {
    var profile = getJSONPref("loop.fxa_oauth.tokendata") && 
    getJSONPref("loop.fxa_oauth.profile");

    return profile;}, 


  /**
   * Gets the encryption key for this profile.
   */
  promiseProfileEncryptionKey: function promiseProfileEncryptionKey() {var _this10 = this;
    return new Promise(function (resolve, reject) {
      if (_this10.userProfile) {
        // We're an FxA user.
        if (Services.prefs.prefHasUserValue("loop.key.fxa")) {
          resolve(MozLoopService.getLoopPref("key.fxa"));
          return;}


        // This should generally never happen, but its not really possible
        // for us to force reauth from here in a sensible way for the user.
        // So we'll just have to flag it the best we can.
        reject(new Error("No FxA key available"));
        return;}


      // XXX Temporarily save in preferences until we've got some
      // extra storage (bug 1152761).
      if (!Services.prefs.prefHasUserValue("loop.key")) {
        // Get a new value.
        loopCrypto.generateKey().then(function (key) {
          Services.prefs.setCharPref("loop.key", key);
          resolve(key);}).
        catch(function (error) {
          MozLoopService.log.error(error);
          reject(error);});

        return;}


      resolve(MozLoopService.getLoopPref("key"));});}, 



  /**
   * Returns true if this profile has an encryption key. For guest profiles
   * this is always true, since we can generate a new one if needed. For FxA
   * profiles, we need to check the preference.
   *
   * @return {Boolean} True if the profile has an encryption key.
   */
  get hasEncryptionKey() {
    return !this.userProfile || 
    Services.prefs.prefHasUserValue("loop.key.fxa");}, 


  get errors() {
    return MozLoopServiceInternal.errors;}, 


  get log() {
    return log;}, 


  /**
   * Returns the current locale
   *
   * @return {String} The code of the current locale.
   */
  get locale() {
    try {
      return Services.prefs.getComplexValue("general.useragent.locale", 
      Ci.nsISupportsString).data;} 
    catch (ex) {
      return "en-US";}}, 



  /*
   * Returns current FTU version
   *
   * @return {Number}
   *
   * XXX must match number in panel.jsx; expose this via MozLoopAPI
   * and kill that constant.
   */
  get FTU_VERSION() 
  {
    return 2;}, 


  /**
   * Set any preference under "loop.".
   *
   * @param {String} prefSuffix The name of the pref without the preceding "loop."
   * @param {*} value The value to set.
   * @param {Enum} prefType Type of preference, defined at Ci.nsIPrefBranch. Optional.
   *
   * Any errors thrown by the Mozilla pref API are logged to the console.
   */
  setLoopPref: function setLoopPref(prefSuffix, value, prefType) {
    var prefName = "loop." + prefSuffix;
    try {
      if (!prefType) {
        prefType = Services.prefs.getPrefType(prefName);}

      switch (prefType) {
        case Ci.nsIPrefBranch.PREF_STRING:
          Services.prefs.setCharPref(prefName, value);
          break;
        case Ci.nsIPrefBranch.PREF_INT:
          Services.prefs.setIntPref(prefName, value);
          break;
        case Ci.nsIPrefBranch.PREF_BOOL:
          Services.prefs.setBoolPref(prefName, value);
          break;
        default:
          log.error("invalid preference type setting " + prefName);
          break;}} 

    catch (ex) {
      log.error("setLoopPref had trouble setting " + prefName + 
      "; exception: " + ex);}}, 



  /**
   * Return any preference under "loop.".
   *
   * @param {String} prefName The name of the pref without the preceding
   * "loop."
   * @param {Enum} prefType Type of preference, defined at Ci.nsIPrefBranch. Optional.
   *
   * Any errors thrown by the Mozilla pref API are logged to the console
   * and cause null to be returned. This includes the case of the preference
   * not being found.
   *
   * @return {*} on success, null on error
   */
  getLoopPref: function getLoopPref(prefSuffix, prefType) {
    var prefName = "loop." + prefSuffix;
    try {
      if (!prefType) {
        prefType = Services.prefs.getPrefType(prefName);} else 
      if (prefType != Services.prefs.getPrefType(prefName)) {
        log.error("invalid type specified for preference");
        return null;}

      switch (prefType) {
        case Ci.nsIPrefBranch.PREF_STRING:
          return Services.prefs.getCharPref(prefName);
        case Ci.nsIPrefBranch.PREF_INT:
          return Services.prefs.getIntPref(prefName);
        case Ci.nsIPrefBranch.PREF_BOOL:
          return Services.prefs.getBoolPref(prefName);
        default:
          log.error("invalid preference type getting " + prefName);
          return null;}} 

    catch (ex) {
      log.error("getLoopPref had trouble getting " + prefName + 
      "; exception: " + ex);
      return null;}}, 



  /**
   * Start the FxA login flow using the OAuth client and params from the Loop server.
   *
   * The caller should be prepared to handle rejections related to network, server or login errors.
   *
   * @param {Boolean} forceReAuth Set to true to force the user to reauthenticate.
   * @return {Promise} that resolves when the FxA login flow is complete.
   */
  logInToFxA: function logInToFxA(forceReAuth) {
    log.debug("logInToFxA with fxAOAuthTokenData:", !!MozLoopServiceInternal.fxAOAuthTokenData);
    if (!forceReAuth && MozLoopServiceInternal.fxAOAuthTokenData) {
      return Promise.resolve(MozLoopServiceInternal.fxAOAuthTokenData);}

    return MozLoopServiceInternal.promiseFxAOAuthAuthorization(forceReAuth).then(function (response) {return (
        MozLoopServiceInternal.promiseFxAOAuthToken(response.code, response.state));}).
    then(function (tokenData) {
      MozLoopServiceInternal.fxAOAuthTokenData = tokenData;
      return MozLoopServiceInternal.promiseRegisteredWithServers(LOOP_SESSION_TYPE.FXA).then(function () {
        MozLoopServiceInternal.clearError("login");
        MozLoopServiceInternal.clearError("profile");
        return MozLoopServiceInternal.fxAOAuthTokenData;});}).

    then(Task.async(function* fetchProfile(tokenData) {
      yield MozLoopService.fetchFxAProfile(tokenData);
      return tokenData;})).
    catch(function (error) {
      MozLoopServiceInternal.fxAOAuthTokenData = null;
      MozLoopServiceInternal.fxAOAuthProfile = null;
      MozLoopServiceInternal.deferredRegistrations.delete(LOOP_SESSION_TYPE.FXA);
      throw error;}).
    catch(function (error) {
      MozLoopServiceInternal.setError("login", error, 
      function () {return MozLoopService.logInToFxA();});
      // Re-throw for testing
      throw error;});}, 



  /**
   * Logs the user out from FxA.
   *
   * Gracefully handles if the user is already logged out.
   *
   * @return {Promise} that resolves when the FxA logout flow is complete.
   */
  logOutFromFxA: Task.async(function* () {
    log.debug("logOutFromFxA");
    try {
      yield MozLoopServiceInternal.unregisterFromLoopServer(LOOP_SESSION_TYPE.FXA);} 

    catch (err) {
      throw err;} finally 

    {
      MozLoopServiceInternal.clearSessionToken(LOOP_SESSION_TYPE.FXA);
      MozLoopServiceInternal.fxAOAuthTokenData = null;
      MozLoopServiceInternal.fxAOAuthProfile = null;
      MozLoopServiceInternal.deferredRegistrations.delete(LOOP_SESSION_TYPE.FXA);
      // Unregister with PushHandler so these push channels will not get re-registered
      // if the connection is re-established by the PushHandler.
      MozLoopServiceInternal.pushHandler.unregister(MozLoopService.channelIDs.roomsFxA);

      // Reset the client since the initial promiseFxAOAuthParameters() call is
      // what creates a new session.
      gFxAOAuthClient = null;
      gFxAOAuthClientPromise = null;

      // clearError calls notifyStatusChanged so should be done last when the
      // state is clean.
      MozLoopServiceInternal.clearError("registration");
      MozLoopServiceInternal.clearError("login");
      MozLoopServiceInternal.clearError("profile");}}), 



  /**
   * Fetch/update the FxA Profile for the logged in user.
   *
   * @return {Promise} resolving if the profile information was succesfully retrieved
   *                   rejecting if the profile information couldn't be retrieved.
   *                   A profile error is registered.
   **/
  fetchFxAProfile: function fetchFxAProfile() {var _this11 = this;
    log.debug("fetchFxAProfile");
    var client = new FxAccountsProfileClient({ 
      serverURL: gFxAOAuthClient.parameters.profile_uri, 
      token: MozLoopServiceInternal.fxAOAuthTokenData.access_token });

    return client.fetchProfile().then(function (result) {
      MozLoopServiceInternal.fxAOAuthProfile = result;
      MozLoopServiceInternal.clearError("profile");}, 
    function (error) {
      log.error("Failed to retrieve profile", error, _this11.fetchFxAProfile.bind(_this11));
      MozLoopServiceInternal.setError("profile", error);
      MozLoopServiceInternal.fxAOAuthProfile = null;
      MozLoopServiceInternal.notifyStatusChanged();});}, 



  openFxASettings: Task.async(function* () {
    try {
      var fxAOAuthClient = yield MozLoopServiceInternal.promiseFxAOAuthClient();
      if (!fxAOAuthClient) {
        log.error("Could not get the OAuth client");
        return;}

      var url = new URL("/settings", fxAOAuthClient.parameters.content_uri);

      if (this.userProfile) {
        // fxA User profile is present, open settings for the correct profile. Bug: 1070208
        var fxAProfileUid = MozLoopService.userProfile.uid;
        url = new URL("/settings?uid=" + fxAProfileUid, fxAOAuthClient.parameters.content_uri);}


      var win = Services.wm.getMostRecentWindow("navigator:browser");
      win.switchToTabHavingURI(url.toString(), true);} 
    catch (ex) {
      log.error("Error opening FxA settings", ex);}}), 



  /**
   * Gets the tour URL.
   *
   * @param {String} aSrc A string representing the entry point to begin the tour, optional.
   * @param {Object} aAdditionalParams An object with keys used as query parameter names
   */
  getTourURL: function getTourURL() {var aSrc = arguments.length <= 0 || arguments[0] === undefined ? null : arguments[0];var aAdditionalParams = arguments.length <= 1 || arguments[1] === undefined ? {} : arguments[1];
    var urlStr = this.getLoopPref("gettingStarted.url");
    var url = new URL(Services.urlFormatter.formatURL(urlStr));
    Object.keys(aAdditionalParams).forEach(function (paramName) {
      url.searchParams.set(paramName, aAdditionalParams[paramName]);});

    if (aSrc) {
      url.searchParams.set("utm_source", "firefox-browser");
      url.searchParams.set("utm_medium", "firefox-browser");
      url.searchParams.set("utm_campaign", aSrc);}


    // Find the most recent pageID that has the Loop prefix.
    var mostRecentLoopPageID = { id: null, lastSeen: null };var _iteratorNormalCompletion7 = true;var _didIteratorError7 = false;var _iteratorError7 = undefined;try {
      for (var _iterator7 = UITour.pageIDsForSession[Symbol.iterator](), _step7; !(_iteratorNormalCompletion7 = (_step7 = _iterator7.next()).done); _iteratorNormalCompletion7 = true) {var pageID = _step7.value;
        if (pageID[0] && pageID[0].startsWith("hello-tour_OpenPanel_") && 
        pageID[1] && pageID[1].lastSeen > mostRecentLoopPageID.lastSeen) {
          mostRecentLoopPageID.id = pageID[0];
          mostRecentLoopPageID.lastSeen = pageID[1].lastSeen;}}} catch (err) {_didIteratorError7 = true;_iteratorError7 = err;} finally {try {if (!_iteratorNormalCompletion7 && _iterator7.return) {_iterator7.return();}} finally {if (_didIteratorError7) {throw _iteratorError7;}}}



    var PAGE_ID_EXPIRATION_MS = 60 * 60 * 1000;
    if (mostRecentLoopPageID.id && 
    mostRecentLoopPageID.lastSeen > Date.now() - PAGE_ID_EXPIRATION_MS) {
      url.searchParams.set("utm_content", mostRecentLoopPageID.id);}

    return url;}, 


  resumeTour: function resumeTour(aIncomingConversationState) {
    if (!this.getLoopPref("gettingStarted.resumeOnFirstJoin")) {
      return;}


    var url = this.getTourURL("resume-with-conversation", { 
      incomingConversation: aIncomingConversationState });


    var win = Services.wm.getMostRecentWindow("navigator:browser");

    this.setLoopPref("gettingStarted.resumeOnFirstJoin", false);

    // The query parameters of the url can vary but we always want to re-use a Loop tour tab that's
    // already open so we ignore the fragment and query string.
    var hadExistingTab = win.switchToTabHavingURI(url, true, { 
      ignoreFragment: true, 
      ignoreQueryString: true });


    // If the tab was already open, send an event instead of using the query
    // parameter above (that we don't replace on existing tabs to avoid a reload).
    if (hadExistingTab) {
      UITour.notify("Loop:IncomingConversation", { 
        conversationOpen: aIncomingConversationState === "open" });}}, 




  /**
   * Opens the Getting Started tour in the browser.
   */
  openGettingStartedTour: Task.async(function () {
    var kNSXUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

    // User will have _just_ clicked the tour menu item or the FTU
    // button in the panel, (or else it wouldn't be visible), so...
    var xulWin = Services.wm.getMostRecentWindow("navigator:browser");
    var xulDoc = xulWin.document;

    var box = xulDoc.createElementNS(kNSXUL, "box");
    box.setAttribute("id", "loop-slideshow-container");

    var appContent = xulDoc.getElementById("appcontent");
    var tabBrowser = xulDoc.getElementById("content");
    appContent.insertBefore(box, tabBrowser);

    var xulBrowser = xulDoc.createElementNS(kNSXUL, "browser");
    xulBrowser.setAttribute("id", "loop-slideshow-browser");
    xulBrowser.setAttribute("flex", "1");
    xulBrowser.setAttribute("type", "content");
    box.appendChild(xulBrowser);

    // Notify the UI, which has the side effect of disabling panel opening
    // and updating the toolbar icon to visually indicate difference.
    xulWin.LoopUI.isSlideshowOpen = true;

    var removeSlideshow = function () {
      try {
        appContent.removeChild(box);} 
      catch (ex) {
        log.error(ex);}


      this.setLoopPref("gettingStarted.latestFTUVersion", this.FTU_VERSION);

      // Notify the UI, which has the side effect of re-enabling panel opening
      // and updating the toolbar.
      xulWin.LoopUI.isSlideshowOpen = false;
      xulWin.LoopUI.openPanel();

      xulWin.removeEventListener("CloseSlideshow", removeSlideshow);

      log.info("slideshow removed");}.
    bind(this);

    function xulLoadListener() {
      xulBrowser.contentWindow.addEventListener("CloseSlideshow", 
      removeSlideshow);
      log.info("CloseSlideshow handler added");

      xulBrowser.removeEventListener("load", xulLoadListener, true);}


    xulBrowser.addEventListener("load", xulLoadListener, true);

    // XXX we are loading the slideshow page with chrome privs.
    // To make this remote, we'll need to think through a better
    // security model.
    xulBrowser.setAttribute("src", 
    "chrome://loop/content/panels/slideshow.html");}), 


  /**
   * Opens a URL in a new tab in the browser.
   *
   * @param {String} url The new url to open
   */
  openURL: function openURL(url) {
    var win = Services.wm.getMostRecentWindow("navigator:browser");
    win.openUILinkIn(Services.urlFormatter.formatURL(url), "tab");}, 


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
  hawkRequest: function hawkRequest(sessionType, path, method, payloadObj) {
    return MozLoopServiceInternal.hawkRequest(sessionType, path, method, payloadObj).catch(
    function (error) {return MozLoopServiceInternal._hawkRequestError(error);});}, 


  /**
   * Returns the window data for a specific conversation window id.
   *
   * This data will be relevant to the type of window, e.g. rooms.
   * See LoopRooms for more information.
   *
   * @param {String} conversationWindowId
   * @returns {Object} The window data or null if error.
   */
  getConversationWindowData: function getConversationWindowData(conversationWindowId) {
    if (gConversationWindowData.has(conversationWindowId)) {
      var conversationData = gConversationWindowData.get(conversationWindowId);
      gConversationWindowData.delete(conversationWindowId);
      return conversationData;}


    log.error("Window data was already fetched before. Possible race condition!");
    return null;}, 


  getConversationContext: function getConversationContext(winId) {
    return MozLoopServiceInternal.conversationContexts.get(winId);}, 


  addConversationContext: function addConversationContext(windowId, context) {
    MozLoopServiceInternal.conversationContexts.set(windowId, context);}, 


  /**
   * Used to record the screen sharing state for a window so that it can
   * be reflected on the toolbar button.
   *
   * @param {String} windowId The id of the conversation window the state
   *                          is being changed for.
   * @param {Boolean} active  Whether or not screen sharing is now active.
   */
  setScreenShareState: function setScreenShareState(windowId, active) {
    if (active) {
      this._activeScreenShares.add(windowId);} else 
    if (this._activeScreenShares.has(windowId)) {
      this._activeScreenShares.delete(windowId);}


    MozLoopServiceInternal.notifyStatusChanged();}, 


  /**
   * Returns true if screen sharing is active in at least one window.
   */
  get screenShareActive() {
    return this._activeScreenShares.size > 0;} };
