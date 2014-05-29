/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
let console = (Cu.import("resource://gre/modules/devtools/Console.jsm", {})).console;

this.EXPORTED_SYMBOLS = ["MozLoopService"];

XPCOMUtils.defineLazyModuleGetter(this, "injectLoopAPI",
  "resource:///modules/loop/MozLoopAPI.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Chat", "resource:///modules/Chat.jsm");

/**
 * We don't have push notifications on desktop currently, so this is a
 * workaround to get them going for us.
 *
 * XXX Handle auto-reconnections if connection fails for whatever reason
 * (bug 1013248).
 */
let PushHandlerHack = {
  // This is the uri of the push server.
  pushServerUri: Services.prefs.getCharPref("services.push.serverURL"),
  // This is the channel id we're using for notifications
  channelID: "8b1081ce-9b35-42b5-b8f5-3ff8cb813a50",
  // Stores the push url if we're registered and we have one.
  pushUrl: undefined,

  /**
   * Call to start the connection to the push socket server. On
   * connection, it will automatically say hello and register the channel
   * id with the server.
   *
   * Register callback parameters:
   * - {String|null} err: Encountered error, if any
   * - {String} url: The push url obtained from the server
   *
   * @param {Function} registerCallback Callback to be called once we are
   *                     registered.
   * @param {Function} notificationCallback Callback to be called when a
   *                     push notification is received.
   */
  initialize: function(registerCallback, notificationCallback) {
    if (Services.io.offline) {
      registerCallback("offline");
      return;
    }

    this._registerCallback = registerCallback;
    this._notificationCallback = notificationCallback;

    this.websocket = Cc["@mozilla.org/network/protocol;1?name=wss"]
                       .createInstance(Ci.nsIWebSocketChannel);

    this.websocket.protocol = "push-notification";

    var pushURI = Services.io.newURI(this.pushServerUri, null, null);
    this.websocket.asyncOpen(pushURI, this.pushServerUri, this, null);
  },

  /**
   * Listener method, handles the start of the websocket stream.
   * Sends a hello message to the server.
   *
   * @param {nsISupports} aContext Not used
   */
  onStart: function() {
    var helloMsg = { messageType: "hello", uaid: "", channelIDs: [] };
    this.websocket.sendMsg(JSON.stringify(helloMsg));
  },

  /**
   * Listener method, called when the websocket is closed.
   *
   * @param {nsISupports} aContext Not used
   * @param {nsresult} aStatusCode Reason for stopping (NS_OK = successful)
   */
  onStop: function(aContext, aStatusCode) {
    // XXX We really should be handling auto-reconnect here, this will be
    // implemented in bug 994151. For now, just log a warning, so that a
    // developer can find out it has happened and not get too confused.
    Cu.reportError("Loop Push server web socket closed! Code: " + aStatusCode);
    this.pushUrl = undefined;
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
  onServerClose: function(aContext, aCode) {
    // XXX We really should be handling auto-reconnect here, this will be
    // implemented in bug 994151. For now, just log a warning, so that a
    // developer can find out it has happened and not get too confused.
    Cu.reportError("Loop Push server web socket closed (server)! Code: " + aCode);
    this.pushUrl = undefined;
  },

  /**
   * Listener method, called when the websocket receives a message.
   *
   * @param {nsISupports} aContext Not used
   * @param {String} aMsg The message data
   */
  onMessageAvailable: function(aContext, aMsg) {
    var msg = JSON.parse(aMsg);

    switch(msg.messageType) {
      case "hello":
        this._registerChannel();
        break;
      case "register":
        this.pushUrl = msg.pushEndpoint;
        this._registerCallback(null, this.pushUrl);
        break;
      case "notification":
        msg.updates.forEach(function(update) {
          if (update.channelID === this.channelID) {
            this._notificationCallback(update.version);
          }
        }.bind(this));
        break;
    }
  },

  /**
   * Handles registering a service
   */
  _registerChannel: function() {
    this.websocket.sendMsg(JSON.stringify({
      messageType: "register",
      channelID: this.channelID
    }));
  }
};

/**
 * Internal helper methods and state
 *
 * The registration is a two-part process. First we need to connect to
 * and register with the push server. Then we need to take the result of that
 * and register with the Loop server.
 */
let MozLoopServiceInternal = {
  // The uri of the Loop server.
  loopServerUri: Services.prefs.getCharPref("loop.server"),

  // Any callbacks needed when the registration process completes.
  registrationCallbacks: [],

  /**
   * The initial delay for push registration. This ensures we don't start
   * kicking off straight after browser startup, just a few seconds later.
   */
  get initialRegistrationDelayMilliseconds() {
    // Default to 5 seconds
    let initialDelay = 5000;
    try {
      // Let a pref override this for developer & testing use.
      initialDelay = Services.prefs.getIntPref("loop.initialDelay");
    } catch (x) {
      // It is ok for the pref not to exist.
    }
    return initialDelay;
  },

  /**
   * Gets the current latest expiry time for urls.
   *
   * In seconds since epoch.
   */
  get expiryTimeSeconds() {
    let expiryTimeSeconds = 0;
    try {
      expiryTimeSeconds = Services.prefs.getIntPref("loop.urlsExpiryTimeSeconds");
    } catch (x) {
      // It is ok for the pref not to exist.
    }

    return expiryTimeSeconds;
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
   * Starts the initialization of the service, which goes and registers
   * with the push server and the loop server.
   *
   * Callback parameters:
   * - err null on successful initialization and registration,
   *       false if initialization is complete, but registration has not taken
   *             place,
   *       <other> anything else if registration has failed.
   *
   * @param {Function} callback Optional, called when initialization finishes.
   */
  initialize: function(callback) {
    if (this.registeredPushServer || this.initalizeTimer || this.registrationInProgress) {
      if (callback)
        callback(this.registeredPushServer ? null : false);
      return;
    }

    function secondsToMilli(value) {
      return value * 1000;
    }

    // If expiresTime is in the future then kick-off registration.
    if (secondsToMilli(this.expiryTimeSeconds) > Date.now()) {
      // Kick off the push notification service into registering after a timeout
      // this ensures we're not doing too much straight after the browser's finished
      // starting up.
      this.initializeTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      this.initializeTimer.initWithCallback(function() {
        this.registerWithServers(callback);
        this.initializeTimer = null;
      }.bind(this),
      this.initialRegistrationDelayMilliseconds, Ci.nsITimer.TYPE_ONE_SHOT);
    }
    else if (callback) {
      // Callback with false as we haven't needed to do registration.
      callback(false);
    }
  },

  /**
   * Starts registration of Loop with the push server, and then will register
   * with the Loop server. It will return early if already registered.
   *
   * Callback parameters:
   * - err null on successful registration, non-null otherwise.
   *
   * @param {Function} callback Called when the registration process finishes.
   */
  registerWithServers: function(callback) {
    // If we've already registered, return straight away, but save the callback
    // so we can let the caller know.
    if (this.registeredLoopServer) {
      callback(null);
      return;
    }

    // We need to register, so save the callback.
    this.registrationCallbacks.push(callback);

    // If we're already in progress, just return straight away.
    if (this.registrationInProgress) {
      return;
    }

    this.registrationInProgress = true;

    PushHandlerHack.initialize(this.onPushRegistered.bind(this),
                               this.onHandleNotification.bind(this));
  },

  /**
   * Handles the end of the registration process.
   *
   * @param {Object|null} err null on success, non-null otherwise.
   */
  endRegistration: function(err) {
    // Reset the in progress flag
    this.registrationInProgress = false;

    // Call any callbacks, then release them.
    this.registrationCallbacks.forEach(function(callback) {
      callback(err);
    });
    this.registrationCallbacks.length = 0;
  },

  /**
   * Callback from PushHandlerHack - The push server has been registered
   * and has given us a push url.
   *
   * @param {String} pushUrl The push url given by the push server.
   */
  onPushRegistered: function(err, pushUrl) {
    if (err) {
      this.endRegistration(err);
      return;
    }

    this.loopXhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
      .createInstance(Ci.nsIXMLHttpRequest);

    this.loopXhr.open('POST', MozLoopServiceInternal.loopServerUri + "/registration",
                          true);
    this.loopXhr.setRequestHeader('Content-Type', 'application/json');

    this.loopXhr.channel.loadFlags = Ci.nsIChannel.INHIBIT_CACHING
      | Ci.nsIChannel.LOAD_BYPASS_CACHE
      | Ci.nsIChannel.LOAD_EXPLICIT_CREDENTIALS;

    this.loopXhr.onreadystatechange = this.onLoopRegistered.bind(this);

    this.loopXhr.sendAsBinary(JSON.stringify({
      simple_push_url: pushUrl
    }));
  },

  /**
   * Callback from PushHandlerHack - A push notification has been received from
   * the server.
   *
   * @param {String} version The version information from the server.
   */
  onHandleNotification: function(version) {
    this.openChatWindow(null, "LooP", "about:loopconversation#start/" + version);
  },

  /**
   * Callback from the loopXhr. Checks the registration result.
   */
  onLoopRegistered: function() {
    if (this.loopXhr.readyState != Ci.nsIXMLHttpRequest.DONE)
      return;

    let status = this.loopXhr.status;
    if (status != 200) {
      // XXX Bubble the precise details up to the UI somehow (bug 1013248).
      Cu.reportError("Failed to register with the loop server. Code: " +
        status + " Text: " + this.loopXhr.statusText);
      this.endRegistration(status);
      return;
    }

    let sessionToken = this.loopXhr.getResponseHeader("Hawk-Session-Token");
    if (sessionToken !== null) {

      // XXX should do more validation here
      if (sessionToken.length === 64) {

        Services.prefs.setCharPref("loop.hawk-session-token", sessionToken);
      } else {
        // XXX Bubble the precise details up to the UI somehow (bug 1013248).
        console.warn("Loop server sent an invalid session token");
        this.endRegistration("session-token-wrong-size");
        return;
      }
    }

    // If we made it this far, we registered just fine.
    this.registeredLoopServer = true;
    this.endRegistration(null);
  },

  /**
   * A getter to obtain and store the strings for loop. This is structured
   * for use by l10n.js.
   *
   * @returns {Object} a map of element ids with attributes to set.
   */
  get localizedStrings() {
    if (this._localizedStrings)
      return this._localizedStrings;

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

    return this._localizedStrings = map;
  },

  /**
   * Opens the chat window
   *
   * @param {Object} contentWindow The window to open the chat window in, may
   *                               be null.
   * @param {String} title The title of the chat window.
   * @param {String} url The page to load in the chat window.
   * @param {String} mode May be "minimized" or undefined.
   */
  openChatWindow: function(contentWindow, title, url, mode) {
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

      chatbox.addEventListener("DOMContentLoaded", function loaded(event) {
        if (event.target != chatbox.contentDocument) {
          return;
        }
        chatbox.removeEventListener("DOMContentLoaded", loaded, true);
        injectLoopAPI(chatbox.contentWindow);
      }, true);
    };

    Chat.open(contentWindow, origin, title, url, undefined, undefined, callback);
  }
};

/**
 * Public API
 */
this.MozLoopService = {
  /**
   * Initialized the loop service, and starts registration with the
   * push and loop servers.
   *
   * Callback parameters:
   * - err null on successful initialization and registration,
   *       false if initialization is complete, but registration has not taken
   *             place,
   *       <other> anything else if registration has failed.
   *
   * @param {Function} callback Optional, called when initialization finishes.
   */
  initialize: function(callback) {
    MozLoopServiceInternal.initialize(callback);
  },

  /**
   * Starts registration of Loop with the push server, and then will register
   * with the Loop server. It will return early if already registered.
   *
   * Callback parameters:
   * - err null on successful registration, non-null otherwise.
   *
   * @param {Function} callback Called when the registration process finishes.
   */
  register: function(callback) {
    MozLoopServiceInternal.registerWithServers(callback);
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
      console.log("getLoopCharPref had trouble getting " + prefName +
        "; exception: " + ex);
      return null;
    }
  }
};
