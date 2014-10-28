/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

this.EXPORTED_SYMBOLS = ["MozLoopPushHandler"];

XPCOMUtils.defineLazyModuleGetter(this, "console",
                                  "resource://gre/modules/devtools/Console.jsm");

/**
 * We don't have push notifications on desktop currently, so this is a
 * workaround to get them going for us.
 */
let MozLoopPushHandler = {
  // This is the uri of the push server.
  pushServerUri: undefined,
  // Records containing the registration and notification callbacks indexed by channelID.
  // Each channel will be registered with the PushServer.
  channels: {},
  // This is the UserAgent UUID assigned by the PushServer
  uaID: undefined,
  // Each successfully registered channelID is used as a key to hold its pushEndpoint URL.
  registeredChannels: {},

  _channelsToRegister: {},

  _minRetryDelay_ms: (() => {
    try {
      return Services.prefs.getIntPref("loop.retry_delay.start")
    }
    catch (e) {
      return 60000 // 1 minute
    }
  })(),

  _maxRetryDelay_ms: (() => {
    try {
      return Services.prefs.getIntPref("loop.retry_delay.limit")
    }
    catch (e) {
      return 300000 // 5 minutes
    }
  })(),

   /**
    * Inializes the PushHandler and opens a socket with the PushServer.
    * It will automatically say hello and register any channels
    * that are found in the work queue at that point.
    *
    * @param {Object} options Set of configuration options. Currently,
    *                 the only option is mocketWebSocket which will be
    *                 used for testing.
    */
  initialize: function(options = {}) {
    if (Services.io.offline) {
      console.warn("MozLoopPushHandler - IO offline");
      return false;
    }

    if (this._initDone) {
      return true;
    }

    this._initDone = true;

    if ("mockWebSocket" in options) {
      this._mockWebSocket = options.mockWebSocket;
    }

    this._openSocket();
    return true;
  },

   /**
    * Start registration of a PushServer notification channel.
    * connection, it will automatically say hello and register the channel
    * id with the server.
    *
    * onRegistered callback parameters:
    * - {String|null} err: Encountered error, if any
    * - {String} url: The push url obtained from the server
    *
    * onNotification parameters:
    * - {String} version The version string received from the push server for
    *                    the notification.
    * - {String} channelID The channelID on which the notification was sent.
    *
    * @param {String} channelID Channel ID to use in registration.
    *
    * @param {Function} onRegistered Callback to be called once we are
    *                     registered.
    * @param {Function} onNotification Callback to be called when a
    *                     push notification is received (may be called multiple
    *                     times).
    */
  register: function(channelID, onRegistered, onNotification) {
    if (!channelID || !onRegistered || !onNotification) {
      throw new Error("missing required parameter(s):"
                      + (channelID ? "" : " channelID")
                      + (onRegistered ? "" : " onRegistered")
                      + (onNotification ? "" : " onNotification"));
    }

    // If the channel is already registered, callback with an error immediately
    // so we don't leave code hanging waiting for an onRegistered callback.
    if (channelID in this.channels) {
      onRegistered("error: channel already registered: " + channelID);
      return;
    }

    this.channels[channelID] = {
      onRegistered: onRegistered,
      onNotification: onNotification
    };

    // If registration is in progress, simply add to the work list.
    // Else, re-start a registration cycle.
    if (this._registrationID) {
      this._channelsToRegister.push(channelID);
    } else {
      this._registerChannels();
    }
  },

  /**
   * Listener method, handles the start of the websocket stream.
   * Sends a hello message to the server.
   *
   * @param {nsISupports} aContext Not used
   */
  onStart: function() {
    this._retryEnd();
    // If a uaID has already been assigned, assume this is a re-connect
    // and send the uaID in order to re-synch with the
    // PushServer. If a registration has been completed, send the channelID.
    let helloMsg = {
          messageType: "hello",
          uaid: this.uaID || "",
          channelIDs: Object.keys(this.registeredChannels)};

    this._retryOperation(() => this.onStart(), this._maxRetryDelay_ms);
    try { // in case websocket has closed before this handler is run
      this._websocket.sendMsg(JSON.stringify(helloMsg));
    }
    catch (e) {console.warn("MozLoopPushHandler::onStart websocket.sendMsg() failure");}
  },

  /**
   * Listener method, called when the websocket is closed.
   *
   * @param {nsISupports} aContext Not used
   * @param {nsresult} aStatusCode Reason for stopping (NS_OK = successful)
   */
  onStop: function(aContext, aStatusCode) {
    Cu.reportError("Loop Push server web socket closed! Code: " + aStatusCode);
    this._retryOperation(() => this._openSocket());
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
    Cu.reportError("Loop Push server web socket closed (server)! Code: " + aCode);
    this._retryOperation(() => this._openSocket());
  },

  /**
   * Listener method, called when the websocket receives a message.
   *
   * @param {nsISupports} aContext Not used
   * @param {String} aMsg The message data
   */
  onMessageAvailable: function(aContext, aMsg) {
    let msg = JSON.parse(aMsg);

    switch(msg.messageType) {
      case "hello":
        this._retryEnd();
        this._isConnected = true;
        if (this.uaID !== msg.uaid) {
          this.uaID = msg.uaid;
          this.registeredChannels = {};
          this._registerChannels();
        }
        break;

      case "register":
        this._onRegister(msg);
        break;

      case "notification":
        msg.updates.forEach((update) => {
          if (update.channelID in this.registeredChannels) {
            this.channels[update.channelID].onNotification(update.version, update.channelID);
          }
        });
        break;
    }
  },

  /**
   * Handles the PushServer registration response.
   *
   * @param {Object} msg PushServer to UserAgent registration response (parsed from JSON).
   */
  _onRegister: function(msg) {
    let registerNext = () => {
      this._registrationID = this._channelsToRegister.shift();
      this._sendRegistration(this._registrationID);
    }

    switch (msg.status) {
      case 200:
        if (msg.channelID == this._registrationID) {
          this._retryEnd(); // reset retry mechanism
          this.registeredChannels[msg.channelID] = msg.pushEndpoint;
          this.channels[msg.channelID].onRegistered(null, msg.pushEndpoint, msg.channelID);
          registerNext();
        }
        break;

      case 500:
        // retry the registration request after a suitable delay
        this._retryOperation(() => this._sendRegistration(msg.channelID));
        break;

      case 409:
        this.channels[this._registrationID].onRegistered(
          "error: PushServer ChannelID already in use: " + msg.channelID);
        registerNext();
        break;

      default:
        let id = this._channelsToRegister.shift();
        this.channels[this._registrationID].onRegistered(
          "error: PushServer registration failure, status = " + msg.status);
        registerNext();
        break;
    }
  },

  /**
   * Attempts to open a websocket.
   *
   * A new websocket interface is used each time. If an onStop callback
   * was received, calling asyncOpen() on the same interface will
   * trigger a "alreay open socket" exception even though the channel
   * is logically closed.
   */
  _openSocket: function() {
    this._isConnected = false;

    if (this._mockWebSocket) {
      // For tests, use the mock instance.
      this._websocket = this._mockWebSocket;
    } else {
      this._websocket = Cc["@mozilla.org/network/protocol;1?name=wss"]
                        .createInstance(Ci.nsIWebSocketChannel);
    }

    this._websocket.protocol = "push-notification";

    let performOpen = () => {
      let uri = Services.io.newURI(this.pushServerUri, null, null);
      this._websocket.asyncOpen(uri, this.pushServerUri, this, null);
    }

    let pushServerURLFetchError = () => {
      console.warn("MozLoopPushHandler - Could not retrieve push server URL from Loop server; using default");
      this.pushServerUri = Services.prefs.getCharPref("services.push.serverURL");
      performOpen();
    }

    if (!this.pushServerUri) {
      // Get push server to use from the Loop server
      let pushUrlEndpoint = Services.prefs.getCharPref("loop.server") + "/push-server-config";
      let req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
                createInstance(Ci.nsIXMLHttpRequest);
      req.open("GET", pushUrlEndpoint);
      req.onload = () => {
        if (req.status >= 200 && req.status < 300) {
          let pushServerConfig;
          try {
            pushServerConfig = JSON.parse(req.responseText);
          } catch (e) {
            console.warn("MozLoopPushHandler - Error parsing JSON response for push server URL");
            pushServerURLFetchError();
          }
          if (pushServerConfig.pushServerURI) {
            this.pushServerUri = pushServerConfig.pushServerURI;
            performOpen();
          } else {
            console.warn("MozLoopPushHandler - push server URL config lacks pushServerURI parameter");
            pushServerURLFetchError();
          }
        } else {
          console.warn("MozLoopPushHandler - push server URL retrieve error: " + req.status);
          pushServerURLFetchError();
        }
      };
      req.onerror = pushServerURLFetchError;
      req.send();
    } else {
      // this.pushServerUri already set -- just open the channel
      performOpen();
    }
  },

  /**
   * Begins registering the channelIDs with the PushServer
   */
  _registerChannels: function() {
    // Hold off registration operation until handshake is complete.
    if (!this._isConnected) {
      return;
    }

    // If a registration is pending, do not generate a work list.
    // Assume registration is in progress.
    if (!this._registrationID) {
      // Generate a list of channelIDs that have not yet been registered.
      this._channelsToRegister = Object.keys(this.channels).filter((id) => {
        return !(id in this.registeredChannels);
      });
      this._registrationID = this._channelsToRegister.shift();
      this._sendRegistration(this._registrationID);
    }
  },

  /**
   * Handles registering a service
   *
   * @param {string} channelID - identification token to use in registration for this channel.
   */
  _sendRegistration: function(channelID) {
    if (channelID) {
      try { // in case websocket has closed
        this._websocket.sendMsg(JSON.stringify({messageType: "register",
                                                channelID: channelID}));
      }
      catch (e) {console.warn("MozLoopPushHandler::_registerChannel websocket.sendMsg() failure");}
    }
  },

  /**
   * Method to handle retrying UserAgent to PushServer request following
   * a retry back-off scheme managed by this function.
   *
   * @param {function} delayedOp Function to call after current delay is satisfied
   *
   * @param {number} [optional] retryDelay This parameter will be used as the initial delay
   */
  _retryOperation: function(delayedOp, retryDelay) {
    if (!this._retryCount) {
      this._retryDelay = retryDelay || this._minRetryDelay_ms;
      this._retryCount = 1;
    } else {
      let nextDelay = this._retryDelay * 2;
      this._retryDelay = nextDelay > this._maxRetryDelay_ms ? this._maxRetryDelay_ms : nextDelay;
      this._retryCount += 1;
    }
    this._timeoutID = setTimeout(delayedOp, this._retryDelay);
  },

  /**
   * Method used to reset the retry delay back-off logic.
   *
   */
  _retryEnd: function() {
    if (this._retryCount) {
      clearTimeout(this._timeoutID);
      this._retryCount = 0;
    }
  }
};
