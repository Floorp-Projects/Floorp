/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.shared = loop.shared || {};
loop.shared.models = (function() {
  "use strict";

  /**
   * Conversation model.
   */
  var ConversationModel = Backbone.Model.extend({
    defaults: {
      ongoing:      false,     // Ongoing call flag
      callerId:     undefined, // Loop caller id
      loopToken:    undefined, // Loop conversation token
      loopVersion:  undefined, // Loop version for /calls/ information. This
                               // is the version received from the push
                               // notification and is used by the server to
                               // determine the pending calls
      sessionId:    undefined, // OT session id
      sessionToken: undefined, // OT session token
      apiKey:       undefined  // OT api key
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
     * Required options:
     * - {OT} sdk: SDK object.
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
    },

    /**
     * Initiates a conversation, requesting call session information to the Loop
     * server and updates appropriately the current model attributes with the
     * data.
     *
     * Available options:
     *
     * - {Boolean} outgoing Set to true if this model represents the
     *                            outgoing call.
     * - {Boolean} callType Only valid for outgoing calls. The type of media in
     *                      the call, e.g. "audio" or "audio-video"
     * - {loop.shared.Client} client  A client object to request call information
     *                                from. Expects requestCallInfo for outgoing
     *                                calls, requestCallsInfo for incoming calls.
     *
     * Triggered events:
     *
     * - `session:ready` when the session information have been successfully
     *   retrieved from the server;
     * - `session:error` when the request failed.
     *
     * @param {Object} options Options object
     */
    initiate: function(options) {
      function handleResult(err, sessionData) {
        /*jshint validthis:true */
        if (err) {
          this.trigger("session:error", new Error(
            "Retrieval of session information failed: HTTP " + err));
          return;
        }

        // XXX For incoming calls we might have more than one call queued.
        // For now, we'll just assume the first call is the right information.
        // We'll probably really want to be getting this data from the
        // background worker on the desktop client.
        // Bug 990714 should fix this.
        if (!options.outgoing)
          sessionData = sessionData[0];

        this.setReady(sessionData);
      }

      if (options.outgoing) {
        options.client.requestCallInfo(this.get("loopToken"), options.callType,
          handleResult.bind(this));
      }
      else {
        options.client.requestCallsInfo(this.get("loopVersion"),
          handleResult.bind(this));
      }
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
     * Sets session information and triggers the `session:ready` event.
     *
     * @param {Object} sessionData Conversation session information.
     */
    setReady: function(sessionData) {
      // Explicit property assignment to prevent later "surprises"
      this.set({
        sessionId:    sessionData.sessionId,
        sessionToken: sessionData.sessionToken,
        apiKey:       sessionData.apiKey
      }).trigger("session:ready", this);
      return this;
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
      this.once("session:ended", this.stopListening, this);
      this.set("ongoing", false);
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
        this.set("ongoing", true);
      }
    },

    /**
     * New created streams are available.
     * http://tokbox.com/opentok/libraries/client/js/reference/StreamEvent.html
     *
     * @param  {StreamEvent} event
     */
    _streamCreated: function(event) {
      this.trigger("session:stream-created", event);
    },

    /**
     * Local user hung up.
     * http://tokbox.com/opentok/libraries/client/js/reference/SessionDisconnectEvent.html
     *
     * @param  {SessionDisconnectEvent} event
     */
    _sessionDisconnected: function(event) {
      this.trigger("session:ended");
      this.set("ongoing", false);
    },

    /**
     * Peer hung up. Disconnects local session.
     * http://tokbox.com/opentok/libraries/client/js/reference/ConnectionEvent.html
     *
     * @param  {ConnectionEvent} event
     */
    _connectionDestroyed: function(event) {
      this.trigger("session:peer-hungup", {
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
      this.trigger("session:network-disconnected");
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
    model: NotificationModel
  });

  return {
    ConversationModel: ConversationModel,
    NotificationCollection: NotificationCollection,
    NotificationModel: NotificationModel
  };
})();
