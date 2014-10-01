/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.CallConnectionWebSocket = (function() {
  "use strict";

  // Response timeout is 5 seconds as per API.
  var kResponseTimeout = 5000;

  /**
   * Handles a websocket specifically for a call connection.
   *
   * There should be one of these created for each call connection.
   *
   * options items:
   * - url             The url of the websocket to connect to.
   * - callId          The call id for the call
   * - websocketToken  The authentication token for the websocket
   *
   * @param {Object} options The options for this websocket.
   */
  function CallConnectionWebSocket(options) {
    this.options = options || {};

    if (!this.options.url) {
      throw new Error("No url in options");
    }
    if (!this.options.callId) {
      throw new Error("No callId in options");
    }
    if (!this.options.websocketToken) {
      throw new Error("No websocketToken in options");
    }

    this._lastServerState = "init";

    // Set loop.debug.sdk to true in the browser, or standalone:
    // localStorage.setItem("debug.websocket", true);
    this._debugWebSocket =
      loop.shared.utils.getBoolPreference("debug.websocket");

    _.extend(this, Backbone.Events);
  };

  CallConnectionWebSocket.prototype = {
    /**
     * Start the connection to the websocket.
     *
     * @return {Promise} A promise that resolves when the websocket
     *                   server connection is open and "hello"s have been
     *                   exchanged. It is rejected if there is a failure in
     *                   connection or the initial exchange of "hello"s.
     */
    promiseConnect: function() {
      var promise = new Promise(
        function(resolve, reject) {
          this.socket = new WebSocket(this.options.url);
          this.socket.onopen = this._onopen.bind(this);
          this.socket.onmessage = this._onmessage.bind(this);
          this.socket.onerror = this._onerror.bind(this);
          this.socket.onclose = this._onclose.bind(this);

          var timeout = setTimeout(function() {
            if (this.connectDetails && this.connectDetails.reject) {
              this.connectDetails.reject("timeout");
              this._clearConnectionFlags();
            }
          }.bind(this), kResponseTimeout);
          this.connectDetails = {
            resolve: resolve,
            reject: reject,
            timeout: timeout
          };
        }.bind(this));

      return promise;
    },

    /**
     * Closes the websocket. This shouldn't be the normal action as the server
     * will normally close the socket. Only in bad error cases, or where we need
     * to close the socket just before closing the window (to avoid an error)
     * should we call this.
     */
    close: function() {
      this.socket.close();
    },

    _clearConnectionFlags: function() {
      clearTimeout(this.connectDetails.timeout);
      delete this.connectDetails;
    },

    /**
     * Internal function called to resolve the connection promise.
     *
     * It will log an error if no promise is found.
     *
     * @param {String} progressState The current state of progress of the
     *                               websocket.
     */
    _completeConnection: function(progressState) {
      if (this.connectDetails && this.connectDetails.resolve) {
        this.connectDetails.resolve(progressState);
        this._clearConnectionFlags();
        return;
      }

      console.error("Failed to complete connection promise - no promise available");
    },

    /**
     * Checks if the websocket is connecting, and rejects the connection
     * promise if appropriate.
     *
     * @param {Object} event The event to reject the promise with if
     *                       appropriate.
     */
    _checkConnectionFailed: function(event) {
      if (this.connectDetails && this.connectDetails.reject) {
        this.connectDetails.reject(event);
        this._clearConnectionFlags();
        return true;
      }

      return false;
    },

    /**
     * Notifies the server that the user has declined the call.
     */
    decline: function() {
      this._send({
        messageType: "action",
        event: "terminate",
        reason: "reject"
      });
    },

    /**
     * Notifies the server that the user has accepted the call.
     */
    accept: function() {
      this._send({
        messageType: "action",
        event: "accept"
      });
    },

    /**
     * Notifies the server that the outgoing media is up, and the
     * incoming media is being received.
     */
    mediaUp: function() {
      this._send({
        messageType: "action",
        event: "media-up"
      });
    },

    /**
     * Notifies the server that the outgoing call is cancelled by the
     * user.
     */
    cancel: function() {
      this._send({
        messageType: "action",
        event: "terminate",
        reason: "cancel"
      });
    },

    /**
     * Sends data on the websocket.
     *
     * @param {Object} data The data to send.
     */
    _send: function(data) {
      this._log("WS Sending", data);

      this.socket.send(JSON.stringify(data));
    },

    /**
     * Used to determine if the server state is in a completed state, i.e.
     * the server has determined the connection is terminated or connected.
     *
     * @return True if the last received state is terminated or connected.
     */
    get _stateIsCompleted() {
      return this._lastServerState === "terminated" ||
             this._lastServerState === "connected";
    },

    /**
     * Called when the socket is open. Automatically sends a "hello"
     * message to the server.
     */
    _onopen: function() {
      // Auto-register with the server.
      this._send({
        messageType: "hello",
        callId: this.options.callId,
        auth: this.options.websocketToken
      });
    },

    /**
     * Called when a message is received from the server.
     *
     * @param {Object} event The websocket onmessage event.
     */
    _onmessage: function(event) {
      var msg;
      try {
        msg = JSON.parse(event.data);
      } catch (x) {
        console.error("Error parsing received message:", x);
        return;
      }

      this._log("WS Receiving", event.data);

      var previousState = this._lastServerState;
      this._lastServerState = msg.state;

      switch(msg.messageType) {
        case "hello":
          this._completeConnection(msg.state);
          break;
        case "progress":
          this.trigger("progress:" + msg.state);
          this.trigger("progress", msg, previousState);
          break;
      }
    },

    /**
     * Called when there is an error on the websocket.
     *
     * @param {Object} event A simple error event.
     */
    _onerror: function(event) {
      this._log("WS Error", event);

      if (!this._stateIsCompleted &&
          !this._checkConnectionFailed(event)) {
        this.trigger("error", event);
      }
    },

    /**
     * Called when the websocket is closed.
     *
     * @param {CloseEvent} event The details of the websocket closing.
     */
    _onclose: function(event) {
      this._log("WS Close", event);

      // If the websocket goes away when we're not in a completed state
      // then its an error. So we either pass it back via the connection
      // promise, or trigger the closed event.
      if (!this._stateIsCompleted &&
          !this._checkConnectionFailed(event)) {
        this.trigger("closed", event);
      }
    },

    /**
     * Logs debug to the console.
     *
     * Parameters: same as console.log
     */
    _log: function() {
      if (this._debugWebSocket) {
        console.log.apply(console, arguments);
      }
    }
  };

  return CallConnectionWebSocket;
})();
