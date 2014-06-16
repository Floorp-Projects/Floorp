/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

this.EXPORTED_SYMBOLS = ["MozLoopPushHandler"];

XPCOMUtils.defineLazyModuleGetter(this, "console",
                                  "resource://gre/modules/devtools/Console.jsm");

/**
 * We don't have push notifications on desktop currently, so this is a
 * workaround to get them going for us.
 *
 * XXX Handle auto-reconnections if connection fails for whatever reason
 * (bug 1013248).
 */
let MozLoopPushHandler = {
  // This is the uri of the push server.
  pushServerUri: Services.prefs.getCharPref("services.push.serverURL"),
  // This is the channel id we're using for notifications
  channelID: "8b1081ce-9b35-42b5-b8f5-3ff8cb813a50",
  // Stores the push url if we're registered and we have one.
  pushUrl: undefined,

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
    if (Services.io.offline) {
      registerCallback("offline");
      return;
    }

    this._registerCallback = registerCallback;
    this._notificationCallback = notificationCallback;

    if (mockPushHandler) {
      // For tests, use the mock instance.
      this._websocket = mockPushHandler;
    } else {
      this._websocket = Cc["@mozilla.org/network/protocol;1?name=wss"]
        .createInstance(Ci.nsIWebSocketChannel);
    }
    this._websocket.protocol = "push-notification";

    let pushURI = Services.io.newURI(this.pushServerUri, null, null);
    this._websocket.asyncOpen(pushURI, this.pushServerUri, this, null);
  },

  /**
   * Listener method, handles the start of the websocket stream.
   * Sends a hello message to the server.
   *
   * @param {nsISupports} aContext Not used
   */
  onStart: function() {
    let helloMsg = { messageType: "hello", uaid: "", channelIDs: [] };
    this._websocket.sendMsg(JSON.stringify(helloMsg));
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
    let msg = JSON.parse(aMsg);

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
    this._websocket.sendMsg(JSON.stringify({
      messageType: "register",
      channelID: this.channelID
    }));
  }
};

