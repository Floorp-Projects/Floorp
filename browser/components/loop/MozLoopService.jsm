/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

this.EXPORTED_SYMBOLS = ["MozLoopService"];

XPCOMUtils.defineLazyModuleGetter(this, "injectLoopAPI",
  "resource:///modules/loop/MozLoopAPI.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Chat", "resource:///modules/Chat.jsm");

/**
 * We don't have push notifications on desktop currently, so this is a
 * workaround to get them going for us.
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
   * @param {Function} registerCallback Callback to be called once we are
   *                     registered.
   * @param {Function} notificationCallback Callback to be called when a
   *                     push notification is received.
   */
  initialize: function(registerCallback, notificationCallback) {
    this.registerCallback = registerCallback;
    this.notificationCallback = notificationCallback;

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
        this.registerCallback(this.pushUrl);
        break;
      case "notification":
        msg.updates.forEach(function(update) {
          if (update.channelID === this.channelID) {
            this.notificationCallback(update.version);
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
 */
let MozLoopServiceInternal = {
  // The uri of the Loop server.
  loopServerUri: Services.prefs.getCharPref("loop.server"),

  /**
   * The initial delay for push registration.
   *
   * XXX We keep this short at the moment, as we don't handle delayed
   * registrations from the user perspective. Bug 994151 will extend this.
   */
  pushRegistrationDelay: 100,

  /**
   * Starts the initialization of the service, which goes and registers
   * with the push server and the loop server.
   */
  initialize: function() {
    if (this.initialized)
      return;

    this.initialized = true;

    // Kick off the push notification service into registering after a timeout
    // this ensures we're not doing too much straight after the browser's finished
    // starting up.
    this.initializeTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this.initializeTimer.initWithCallback(this.registerPushHandler.bind(this),
      this.pushRegistrationDelay, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  /**
   * Starts registration of Loop with the push server.
   */
  registerPushHandler: function() {
    PushHandlerHack.initialize(this.onPushRegistered.bind(this),
                               this.onHandleNotification.bind(this));
  },

  /**
   * Callback from PushHandlerHack - The push server has been registered
   * and has given us a push url.
   *
   * @param {String} pushUrl The push url given by the push server.
   */
  onPushRegistered: function(pushUrl) {
    this.registerXhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
      .createInstance(Ci.nsIXMLHttpRequest);

    this.registerXhr.open('POST', MozLoopServiceInternal.loopServerUri + "/registration",
                          true);
    this.registerXhr.setRequestHeader('Content-Type', 'application/json');

    this.registerXhr.channel.loadFlags = Ci.nsIChannel.INHIBIT_CACHING
      | Ci.nsIChannel.LOAD_BYPASS_CACHE
      | Ci.nsIChannel.LOAD_EXPLICIT_CREDENTIALS;

    this.registerXhr.onreadystatechange = this.onRegistrationResult.bind(this);

    this.registerXhr.sendAsBinary(JSON.stringify({
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
   * Callback from the registation xhr. Checks the registration result.
   */
  onRegistrationResult: function() {
    if (this.registerXhr.readyState != Ci.nsIXMLHttpRequest.DONE)
      return;

    if (this.registerXhr.status != 200) {
      // XXX Bubble this up to the UI somehow, bug 994151 will handle some of this
      Cu.reportError("Failed to register with push server. Code: " +
        this.registerXhr.status + " Text: " + this.registerXhr.statusText);
      return;
    }

    // Otherwise we registered just fine.
    // XXX For now, we'll just save this fact, bug 994151 (again) will make use of
    // this more.
    this.registeredLoopServer = true;
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
   */
  initialize: function() {
    MozLoopServiceInternal.initialize();
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
  }
};
