/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.shared = loop.shared || {};
loop.shared.models = (function(l10n) {
  "use strict";

  /**
   * Conversation model.
   */
  var ConversationModel = Backbone.Model.extend({
    defaults: {
      connected:    false,         // Session connected flag
      ongoing:      false,         // Ongoing call flag
      callerId:     undefined,     // Loop caller id
      loopToken:    undefined,     // Loop conversation token
      loopCallId:   undefined,     // LoopService id for incoming session
      sessionId:    undefined,     // OT session id
      sessionToken: undefined,     // OT session token
      apiKey:       undefined,     // OT api key
      callId:       undefined,     // The callId on the server
      progressURL:  undefined,     // The websocket url to use for progress
      websocketToken: undefined,   // The token to use for websocket auth, this is
                                   // stored as a hex string which is what the server
                                   // requires.
      callType:     undefined,     // The type of incoming call selected by
                                   // other peer ("audio" or "audio-video")
      selectedCallType: undefined, // The selected type for the call that was
                                   // initiated ("audio" or "audio-video")
      callToken:    undefined,     // Incoming call token.
                                   // Used for blocking a call url
      subscribedStream: false,     // Used to indicate that a stream has been
                                   // subscribed to
      publishedStream: false       // Used to indicate that a stream has been
                                   // published
    },

    /**
     * SDK object.
     * @type {OT}
     */
    sdk: undefined,

    /**
     * SDK session object.
     * @type {XXX}
     */
    session: undefined,

    /**
     * Constructor.
     *
     * Options:
     *
     * Required:
     * - {OT} sdk: OT SDK object.
     *
     * @param  {Object} attributes Attributes object.
     * @param  {Object} options    Options object.
     */
    initialize: function(attributes, options) {
      options = options || {};
      if (!options.sdk) {
        throw new Error("missing required sdk");
      }
      this.sdk = options.sdk;

      // Set loop.debug.sdk to true in the browser, or standalone:
      // localStorage.setItem("debug.sdk", true);
      if (loop.shared.utils.getBoolPreference("debug.sdk")) {
        this.sdk.setLogLevel(this.sdk.DEBUG);
      }
    },

    /**
     * Starts an incoming conversation.
     */
    incoming: function() {
      this.trigger("call:incoming");
    },

    /**
     * Used to indicate that an outgoing call should start any necessary
     * set-up.
     */
    setupOutgoingCall: function() {
      this.trigger("call:outgoing:setup");
    },

    /**
     * Starts an outgoing conversation.
     *
     * @param {Object} sessionData The session data received from the
     *                             server for the outgoing call.
     */
    outgoing: function(sessionData) {
      this.setOutgoingSessionData(sessionData);
      this.trigger("call:outgoing");
    },

    /**
     * Checks that the session is ready.
     *
     * @return {Boolean}
     */
    isSessionReady: function() {
      return !!this.get("sessionId");
    },

    /**
     * Sets session information.
     * Session data received by creating an outgoing call.
     *
     * @param {Object} sessionData Conversation session information.
     */
    setOutgoingSessionData: function(sessionData) {
      // Explicit property assignment to prevent later "surprises"
      this.set({
        sessionId:      sessionData.sessionId,
        sessionToken:   sessionData.sessionToken,
        apiKey:         sessionData.apiKey,
        callId:         sessionData.callId,
        progressURL:    sessionData.progressURL,
        websocketToken: sessionData.websocketToken.toString(16)
      });
    },

    /**
     * Sets session information about the incoming call.
     *
     * @param {Object} sessionData Conversation session information.
     */
    setIncomingSessionData: function(sessionData) {
      // Explicit property assignment to prevent later "surprises"
      this.set({
        sessionId:      sessionData.sessionId,
        sessionToken:   sessionData.sessionToken,
        apiKey:         sessionData.apiKey,
        callId:         sessionData.callId,
        progressURL:    sessionData.progressURL,
        websocketToken: sessionData.websocketToken.toString(16),
        callType:       sessionData.callType || "audio-video",
        callToken:      sessionData.callToken
      });
    },

    /**
     * Starts a SDK session and subscribe to call events.
     */
    startSession: function() {
      if (!this.isSessionReady()) {
        throw new Error("Can't start session as it's not ready");
      }
      this.session = this.sdk.initSession(this.get("sessionId"));
      this.listenTo(this.session, "streamCreated", this._streamCreated);
      this.listenTo(this.session, "connectionDestroyed",
                                  this._connectionDestroyed);
      this.listenTo(this.session, "sessionDisconnected",
                                  this._sessionDisconnected);
      this.listenTo(this.session, "networkDisconnected",
                                  this._networkDisconnected);
      this.session.connect(this.get("apiKey"), this.get("sessionToken"),
                           this._onConnectCompletion.bind(this));
    },

    /**
     * Ends current session.
     */
    endSession: function() {
      this.session.disconnect();
      this.set("ongoing", false)
          .once("session:ended", this.stopListening, this);
    },

    /**
     * Helper function to determine if video stream is available for the
     * incoming or outgoing call
     *
     * @param {string} callType Incoming or outgoing call
     */
    hasVideoStream: function(callType) {
      if (callType === "incoming") {
        return this.get("callType") === "audio-video";
      }
      if (callType === "outgoing") {
        return this.get("selectedCallType") === "audio-video";
      }
      return undefined;
    },

    /**
     * Publishes a local stream.
     *
     * @param {Publisher} publisher The publisher object to publish
     *                              to the session.
     */
    publish: function(publisher) {
      this.session.publish(publisher);
      this.set("publishedStream", true);
    },

    /**
     * Subscribes to a remote stream.
     *
     * @param {Stream} stream The remote stream to subscribe to.
     * @param {DOMElement} element The element to display the stream in.
     * @param {Object} config The display properties to set on the stream as
     *                        documented in:
     * https://tokbox.com/opentok/libraries/client/js/reference/Session.html#subscribe
     */
    subscribe: function(stream, element, config) {
      this.session.subscribe(stream, element, config);
      this.set("subscribedStream", true);
    },

    /**
     * Returns true if a stream has been published and a stream has been
     * subscribed to.
     */
    streamsConnected: function() {
      return this.get("publishedStream") && this.get("subscribedStream");
    },

    /**
     * Handle a loop-server error, which has an optional `errno` property which
     * is server error identifier.
     *
     * Triggers the following events:
     *
     * - `session:expired` for expired call urls
     * - `session:error` for other generic errors
     *
     * @param  {Error} err Error object.
     */
    _handleServerError: function(err) {
      switch (err.errno) {
        // loop-server sends 404 + INVALID_TOKEN (errno 105) whenever a token is
        // missing OR expired; we treat this information as if the url is always
        // expired.
        case 105:
          this.trigger("session:expired", err);
          break;
        default:
          this.trigger("session:error", err);
          break;
      }
    },

    /**
     * Manages connection status
     * triggers apropriate event for connection error/success
     * http://tokbox.com/opentok/tutorials/connect-session/js/
     * http://tokbox.com/opentok/tutorials/hello-world/js/
     * http://tokbox.com/opentok/libraries/client/js/reference/SessionConnectEvent.html
     *
     * @param {error|null} error
     */
    _onConnectCompletion: function(error) {
      if (error) {
        this.trigger("session:connection-error", error);
        this.endSession();
      } else {
        this.trigger("session:connected");
        this.set("connected", true);
      }
    },

    /**
     * New created streams are available.
     * http://tokbox.com/opentok/libraries/client/js/reference/StreamEvent.html
     *
     * @param  {StreamEvent} event
     */
    _streamCreated: function(event) {
      this.set("ongoing", true)
          .trigger("session:stream-created", event);
    },

    /**
     * Local user hung up.
     * http://tokbox.com/opentok/libraries/client/js/reference/SessionDisconnectEvent.html
     *
     * @param  {SessionDisconnectEvent} event
     */
    _sessionDisconnected: function(event) {
      this.set("connected", false)
          .set("ongoing", false)
          .trigger("session:ended");
    },

    /**
     * Peer hung up. Disconnects local session.
     * http://tokbox.com/opentok/libraries/client/js/reference/ConnectionEvent.html
     *
     * @param  {ConnectionEvent} event
     */
    _connectionDestroyed: function(event) {
      this.set("connected", false)
          .set("ongoing", false)
          .trigger("session:peer-hungup", {
            connectionId: event.connection.connectionId
          });
      this.endSession();
    },

    /**
     * Network was disconnected.
     * http://tokbox.com/opentok/libraries/client/js/reference/ConnectionEvent.html
     *
     * @param {ConnectionEvent} event
     */
    _networkDisconnected: function(event) {
      this.set("connected", false)
          .set("ongoing", false)
          .trigger("session:network-disconnected");
      this.endSession();
    },
  });

  /**
   * Notification model.
   */
  var NotificationModel = Backbone.Model.extend({
    defaults: {
      level: "info",
      message: ""
    }
  });

  /**
   * Notification collection
   */
  var NotificationCollection = Backbone.Collection.extend({
    model: NotificationModel,

    /**
     * Adds a warning notification to the stack and renders it.
     *
     * @return {String} message
     */
    warn: function(message) {
      this.add({level: "warning", message: message});
    },

    /**
     * Adds a l10n warning notification to the stack and renders it.
     *
     * @param  {String} messageId L10n message id
     */
    warnL10n: function(messageId) {
      this.warn(l10n.get(messageId));
    },

    /**
     * Adds an error notification to the stack and renders it.
     *
     * @return {String} message
     */
    error: function(message) {
      this.add({level: "error", message: message});
    },

    /**
     * Adds a l10n rror notification to the stack and renders it.
     *
     * @param  {String} messageId L10n message id
     */
    errorL10n: function(messageId) {
      this.error(l10n.get(messageId));
    }
  });

  return {
    ConversationModel: ConversationModel,
    NotificationCollection: NotificationCollection,
    NotificationModel: NotificationModel
  };
})(navigator.mozL10n || document.mozL10n);
