/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.OTSdkDriver = (function() {

  var sharedActions = loop.shared.actions;

  /**
   * This is a wrapper for the OT sdk. It is used to translate the SDK events into
   * actions, and instruct the SDK what to do as a result of actions.
   */
  var OTSdkDriver = function(options) {
      if (!options.dispatcher) {
        throw new Error("Missing option dispatcher");
      }
      if (!options.sdk) {
        throw new Error("Missing option sdk");
      }

      this.dispatcher = options.dispatcher;
      this.sdk = options.sdk;

      this.dispatcher.register(this, [
        "setupStreamElements",
        "setMute"
      ]);
  };

  OTSdkDriver.prototype = {
    /**
     * Handles the setupStreamElements action. Saves the required data and
     * kicks off the initialising of the publisher.
     *
     * @param {sharedActions.SetupStreamElements} actionData The data associated
     *   with the action. See action.js.
     */
    setupStreamElements: function(actionData) {
      this.getLocalElement = actionData.getLocalElementFunc;
      this.getRemoteElement = actionData.getRemoteElementFunc;
      this.publisherConfig = actionData.publisherConfig;

      // At this state we init the publisher, even though we might be waiting for
      // the initial connect of the session. This saves time when setting up
      // the media.
      this.publisher = this.sdk.initPublisher(this.getLocalElement(),
        this.publisherConfig,
        this._onPublishComplete.bind(this));
    },

    /**
     * Handles the setMute action. Informs the published stream to mute
     * or unmute audio as appropriate.
     *
     * @param {sharedActions.SetMute} actionData The data associated with the
     *                                           action. See action.js.
     */
    setMute: function(actionData) {
      if (actionData.type === "audio") {
        this.publisher.publishAudio(actionData.enabled);
      } else {
        this.publisher.publishVideo(actionData.enabled);
      }
    },

    /**
     * Connects a session for the SDK, listening to the required events.
     *
     * sessionData items:
     * - sessionId: The OT session ID
     * - apiKey: The OT API key
     * - sessionToken: The token for the OT session
     *
     * @param {Object} sessionData The session data for setting up the OT session.
     */
    connectSession: function(sessionData) {
      this.session = this.sdk.initSession(sessionData.sessionId);

      this.session.on("streamCreated", this._onRemoteStreamCreated.bind(this));
      this.session.on("connectionDestroyed",
        this._onConnectionDestroyed.bind(this));
      this.session.on("sessionDisconnected",
        this._onSessionDisconnected.bind(this));

      // This starts the actual session connection.
      this.session.connect(sessionData.apiKey, sessionData.sessionToken,
        this._onConnectionComplete.bind(this));
    },

    /**
     * Disconnects the sdk session.
     */
    disconnectSession: function() {
      if (this.session) {
        this.session.off("streamCreated", this._onRemoteStreamCreated.bind(this));
        this.session.off("connectionDestroyed",
          this._onConnectionDestroyed.bind(this));
        this.session.off("sessionDisconnected",
          this._onSessionDisconnected.bind(this));

        this.session.disconnect();
        delete this.session;
      }
      if (this.publisher) {
        this.publisher.destroy();
        delete this.publisher;
      }

      // Also, tidy these variables ready for next time.
      delete this._sessionConnected;
      delete this._publisherReady;
      delete this._publishedLocalStream;
      delete this._subscribedRemoteStream;
    },

    /**
     * Called once the session has finished connecting.
     *
     * @param {Error} error An OT error object, null if there was no error.
     */
    _onConnectionComplete: function(error) {
      if (error) {
        console.error("Failed to complete connection", error);
        this.dispatcher.dispatch(new sharedActions.ConnectionFailure({
          reason: "couldNotConnect"
        }));
        return;
      }

      this._sessionConnected = true;
      this._maybePublishLocalStream();
    },

    /**
     * Handles the connection event for a peer's connection being dropped.
     *
     * @param {SessionDisconnectEvent} event The event details
     * https://tokbox.com/opentok/libraries/client/js/reference/SessionDisconnectEvent.html
     */
    _onConnectionDestroyed: function(event) {
      this.dispatcher.dispatch(new sharedActions.RemotePeerDisconnected({
        peerHungup: event.reason === "clientDisconnected"
      }));
    },

    /**
     * Handles the session event for the connection for this client being
     * destroyed.
     *
     * @param {SessionDisconnectEvent} event The event details:
     * https://tokbox.com/opentok/libraries/client/js/reference/SessionDisconnectEvent.html
     */
    _onSessionDisconnected: function(event) {
      // We only need to worry about the network disconnected reason here.
      if (event.reason === "networkDisconnected") {
        this.dispatcher.dispatch(new sharedActions.ConnectionFailure({
          reason: "networkDisconnected"
        }));
      }
    },

    /**
     * Handles the event when the remote stream is created.
     *
     * @param {StreamEvent} event The event details:
     * https://tokbox.com/opentok/libraries/client/js/reference/StreamEvent.html
     */
    _onRemoteStreamCreated: function(event) {
      this.session.subscribe(event.stream,
        this.getRemoteElement(), this.publisherConfig);

      this._subscribedRemoteStream = true;
      if (this._checkAllStreamsConnected()) {
        this.dispatcher.dispatch(new sharedActions.MediaConnected());
      }
    },

    /**
     * Handles the publishing being complete.
     *
     * @param {Error} error An OT error object, null if there was no error.
     */
    _onPublishComplete: function(error) {
      if (error) {
        console.error("Failed to initialize publisher", error);
        this.dispatcher.dispatch(new sharedActions.ConnectionFailure({
          reason: "noMedia"
        }));
        return;
      }

      this._publisherReady = true;
      this._maybePublishLocalStream();
    },

    /**
     * Publishes the local stream if the session is connected
     * and the publisher is ready.
     */
    _maybePublishLocalStream: function() {
      if (this._sessionConnected && this._publisherReady) {
        // We are clear to publish the stream to the session.
        this.session.publish(this.publisher);

        // Now record the fact, and check if we've got all media yet.
        this._publishedLocalStream = true;
        if (this._checkAllStreamsConnected()) {
          this.dispatcher.dispatch(new sharedActions.MediaConnected());
        }
      }
    },

    /**
     * Used to check if both local and remote streams are available
     * and send an action if they are.
     */
    _checkAllStreamsConnected: function() {
      return this._publishedLocalStream &&
        this._subscribedRemoteStream;
    }
  };

  return OTSdkDriver;

})();
