/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Timer.jsm");

this.EXPORTED_SYMBOLS = ["MozLoopPushHandler"];

XPCOMUtils.defineLazyModuleGetter(this, "console",
                                  "resource://gre/modules/devtools/Console.jsm");

/**
 * We don't have push notifications on desktop currently, so this is a
 * workaround to get them going for us.
 */
let MozLoopPushHandler = {
  // This is the uri of the push server.
  pushServerUri: Services.prefs.getCharPref("services.push.serverURL"),
  // This is the channel id we're using for notifications
  channelID: "8b1081ce-9b35-42b5-b8f5-3ff8cb813a50",
  // This is the UserAgent UUID assigned by the PushServer
  uaID: undefined,
  // Stores the push url if we're registered and we have one.
  pushUrl: undefined,
  // Set to true once the channelID has been registered with the PushServer.
  registered: false,

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
    * Starts a connection to the push socket server. On
    * connection, it will automatically say hello and register the channel
    * id with the server.
    *
    * Register callback parameters:
    * - {String|null} err: Encountered error, if any
    * - {String} url: The push url obtained from the server
    *
    * Callback parameters:
    * - {String} version The version string received from the push server for
    *                    the notification.
    *
    * @param {Function} registerCallback Callback to be called once we are
    *                     registered.
    * @param {Function} notificationCallback Callback to be called when a
    *                     push notification is received (may be called multiple
    *                     times).
    * @param {Object} mockPushHandler Optional, test-only object, to allow
    *                                 the websocket to be mocked for tests.
    */
  initialize: function(registerCallback, notificationCallback, mockPushHandler) {
    if (mockPushHandler) {
      this._mockPushHandler = mockPushHandler;
    }

    this._registerCallback = registerCallback;
    this._notificationCallback = notificationCallback;
    this._openSocket();
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
    let helloMsg = { messageType: "hello",
		     uaid: this.uaID,
		     channelIDs: this.registered ? [this.channelID] :[] };
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
	if (this.uaID !== msg.uaid) {
	  this.uaID = msg.uaid;
          this._registerChannel();
	}
        break;

      case "register":
        this._onRegister(msg);
        break;

      case "notification":
        msg.updates.forEach((update) => {
          if (update.channelID === this.channelID) {
            this._notificationCallback(update.version);
          }
        });
        break;
    }
  },

  /**
   * Handles the PushServer registration response.
   *
   * @param {} msg PushServer to UserAgent registration response (parsed from JSON).
   */
  _onRegister: function(msg) {
    switch (msg.status) {
      case 200:
        this._retryEnd(); // reset retry mechanism
	this.registered = true;
        if (this.pushUrl !== msg.pushEndpoint) {
          this.pushUrl = msg.pushEndpoint;
          this._registerCallback(null, this.pushUrl);
        }
        break;

      case 500:
        // retry the registration request after a suitable delay
        this._retryOperation(() => this._registerChannel());
        break;

      case 409:
        this._registerCallback("error: PushServer ChannelID already in use");
	break;

      default:
        this._registerCallback("error: PushServer registration failure, status = " + msg.status);
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
    if (this._mockPushHandler) {
      // For tests, use the mock instance.
      this._websocket = this._mockPushHandler;
    } else if (!Services.io.offline) {
      this._websocket = Cc["@mozilla.org/network/protocol;1?name=wss"]
                        .createInstance(Ci.nsIWebSocketChannel);
    } else {
      this._registerCallback("offline");
      console.warn("MozLoopPushHandler - IO offline");
      return;
    }

    this._websocket.protocol = "push-notification";
    let uri = Services.io.newURI(this.pushServerUri, null, null);
    this._websocket.asyncOpen(uri, this.pushServerUri, this, null);
  },

  /**
   * Handles registering a service
   */
  _registerChannel: function() {
    this.registered = false;
    try { // in case websocket has closed
      this._websocket.sendMsg(JSON.stringify({messageType: "register",
                                              channelID: this.channelID}));
    }
    catch (e) {console.warn("MozLoopPushHandler::_registerChannel websocket.sendMsg() failure");}
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

