/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.OTSdkDriver = (function() {

  var sharedActions = loop.shared.actions;
  var FAILURE_DETAILS = loop.shared.utils.FAILURE_DETAILS;
  var STREAM_PROPERTIES = loop.shared.utils.STREAM_PROPERTIES;
  var SCREEN_SHARE_STATES = loop.shared.utils.SCREEN_SHARE_STATES;

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

      this.connections = {};

      this.dispatcher.register(this, [
        "setupStreamElements",
        "setMute"
      ]);

    /**
     * XXX This is a workaround for desktop machines that do not have a
     * camera installed. As we don't yet have device enumeration, when
     * we do, this can be removed (bug 1138851), and the sdk should handle it.
     */
    if ("isDesktop" in options && options.isDesktop &&
        !window.MediaStreamTrack.getSources) {
      // If there's no getSources function, the sdk defines its own and caches
      // the result. So here we define the "normal" one which doesn't get cached, so
      // we can change it later.
      window.MediaStreamTrack.getSources = function(callback) {
        callback([{kind: "audio"}, {kind: "video"}]);
      };
    }
  };

  OTSdkDriver.prototype = {
    /**
     * Clones the publisher config into a new object, as the sdk modifies the
     * properties object.
     */
    _getCopyPublisherConfig: function() {
      return _.extend({}, this.publisherConfig);
    },

    /**
     * Handles the setupStreamElements action. Saves the required data and
     * kicks off the initialising of the publisher.
     *
     * @param {sharedActions.SetupStreamElements} actionData The data associated
     *   with the action. See action.js.
     */
    setupStreamElements: function(actionData) {
      this.getLocalElement = actionData.getLocalElementFunc;
      this.getScreenShareElementFunc = actionData.getScreenShareElementFunc;
      this.getRemoteElement = actionData.getRemoteElementFunc;
      this.publisherConfig = actionData.publisherConfig;

      this.sdk.on("exception", this._onOTException.bind(this));

      // At this state we init the publisher, even though we might be waiting for
      // the initial connect of the session. This saves time when setting up
      // the media.
      this._publishLocalStreams();
    },

    /**
     * Internal function to publish a local stream.
     * XXX This can be simplified when bug 1138851 is actioned.
     */
    _publishLocalStreams: function() {
      this.publisher = this.sdk.initPublisher(this.getLocalElement(),
        this._getCopyPublisherConfig());
      this.publisher.on("streamCreated", this._onLocalStreamCreated.bind(this));
      this.publisher.on("accessAllowed", this._onPublishComplete.bind(this));
      this.publisher.on("accessDenied", this._onPublishDenied.bind(this));
      this.publisher.on("accessDialogOpened",
        this._onAccessDialogOpened.bind(this));
    },

    /**
     * Forces the sdk into not using video, and starts publishing again.
     * XXX This is part of the work around that will be removed by bug 1138851.
     */
    retryPublishWithoutVideo: function() {
      window.MediaStreamTrack.getSources = function(callback) {
        callback([{kind: "audio"}]);
      };
      this._publishLocalStreams();
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
     * Initiates a screen sharing publisher.
     *
     * options items:
     *  - {String}  videoSource    The type of screen to share. Values of 'screen',
     *                             'window', 'application' and 'browser' are
     *                             currently supported.
     *  - {mixed}   browserWindow  The unique identifier of a browser window. May
     *                             be passed when `videoSource` is 'browser'.
     *  - {Boolean} scrollWithPage Flag to signal that scrolling a page should
     *                             update the stream. May be passed when
     *                             `videoSource` is 'browser'.
     *
     * @param {Object} options Hash containing options for the SDK
     */
    startScreenShare: function(options) {
      // For browser sharing, we store the window Id so that we can avoid unnecessary
      // re-triggers.
      if (options.videoSource === "browser") {
        this._windowId = options.constraints.browserWindow;
      }

      var config = _.extend(this._getCopyPublisherConfig(), options);

      this.screenshare = this.sdk.initPublisher(this.getScreenShareElementFunc(),
        config);
      this.screenshare.on("accessAllowed", this._onScreenShareGranted.bind(this));
      this.screenshare.on("accessDenied", this._onScreenShareDenied.bind(this));
    },

    /**
     * Initiates switching the browser window that is being shared.
     *
     * @param {Integer} windowId  The windowId of the browser.
     */
    switchAcquiredWindow: function(windowId) {
      if (windowId === this._windowId) {
        return;
      }

      this._windowId = windowId;
      this.screenshare._.switchAcquiredWindow(windowId);
    },

    /**
     * Ends an active screenshare session. Return `true` when an active screen-
     * sharing session was ended or `false` when no session is active.
     *
     * @type {Boolean}
     */
    endScreenShare: function() {
      if (!this.screenshare) {
        return false;
      }

      this.session.unpublish(this.screenshare);
      this.screenshare.off("accessAllowed accessDenied");
      this.screenshare.destroy();
      delete this.screenshare;
      delete this._windowId;
      return true;
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

      this.session.on("connectionCreated", this._onConnectionCreated.bind(this));
      this.session.on("streamCreated", this._onRemoteStreamCreated.bind(this));
      this.session.on("streamDestroyed", this._onRemoteStreamDestroyed.bind(this));
      this.session.on("connectionDestroyed",
        this._onConnectionDestroyed.bind(this));
      this.session.on("sessionDisconnected",
        this._onSessionDisconnected.bind(this));
      this.session.on("streamPropertyChanged", this._onStreamPropertyChanged.bind(this));

      // This starts the actual session connection.
      this.session.connect(sessionData.apiKey, sessionData.sessionToken,
        this._onConnectionComplete.bind(this));
    },

    /**
     * Disconnects the sdk session.
     */
    disconnectSession: function() {
      this.endScreenShare();

      if (this.session) {
        this.session.off("streamCreated streamDestroyed connectionDestroyed " +
          "sessionDisconnected streamPropertyChanged");
        this.session.disconnect();
        delete this.session;
      }
      if (this.publisher) {
        this.publisher.off("accessAllowed accessDenied accessDialogOpened streamCreated");
        this.publisher.destroy();
        delete this.publisher;
      }

      // Also, tidy these variables ready for next time.
      delete this._sessionConnected;
      delete this._publisherReady;
      delete this._publishedLocalStream;
      delete this._subscribedRemoteStream;
      this.connections = {};
    },

    /**
     * Oust all users from an ongoing session. This is typically done when a room
     * owner deletes the room.
     *
     * @param {Function} callback Function to be invoked once all connections are
     *                            ousted
     */
    forceDisconnectAll: function(callback) {
      if (!this._sessionConnected) {
        callback();
        return;
      }

      var connectionNames = Object.keys(this.connections);
      if (connectionNames.length === 0) {
        callback();
        return;
      }
      var disconnectCount = 0;
      connectionNames.forEach(function(id) {
        var connection = this.connections[id];
        this.session.forceDisconnect(connection, function() {
          // When all connections have disconnected, call the callback, since
          // we're done.
          if (++disconnectCount === connectionNames.length) {
            callback();
          }
        });
      }, this);
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
          reason: FAILURE_DETAILS.COULD_NOT_CONNECT
        }));
        return;
      }

      this.dispatcher.dispatch(new sharedActions.ConnectedToSdkServers());
      this._sessionConnected = true;
      this._maybePublishLocalStream();
    },

    /**
     * Handles the connection event for a peer's connection being dropped.
     *
     * @param {ConnectionEvent} event The event details
     * https://tokbox.com/opentok/libraries/client/js/reference/ConnectionEvent.html
     */
    _onConnectionDestroyed: function(event) {
      var connection = event.connection;
      if (connection && (connection.id in this.connections)) {
        delete this.connections[connection.id];
      }
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
      var reason;
      switch (event.reason) {
        case "networkDisconnected":
          reason = FAILURE_DETAILS.NETWORK_DISCONNECTED;
          break;
        case "forceDisconnected":
          reason = FAILURE_DETAILS.EXPIRED_OR_INVALID;
          break;
        default:
          // Other cases don't need to be handled.
          return;
      }

      this.dispatcher.dispatch(new sharedActions.ConnectionFailure({
        reason: reason
      }));
    },

    /**
     * Handles the connection event for a newly connecting peer.
     *
     * @param {ConnectionEvent} event The event details
     * https://tokbox.com/opentok/libraries/client/js/reference/ConnectionEvent.html
     */
    _onConnectionCreated: function(event) {
      var connection = event.connection;
      if (this.session.connection.id === connection.id) {
        return;
      }
      this.connections[connection.id] = connection;
      this.dispatcher.dispatch(new sharedActions.RemotePeerConnected());
    },

    /**
     * Handles when a remote screen share is created, subscribing to
     * the stream, and notifying the stores that a share is being
     * received.
     *
     * @param {Stream} stream The SDK Stream:
     * https://tokbox.com/opentok/libraries/client/js/reference/Stream.html
     */
    _handleRemoteScreenShareCreated: function(stream) {
      if (!this.getScreenShareElementFunc) {
        return;
      }

      // Let the stores know first so they can update the display.
      this.dispatcher.dispatch(new sharedActions.ReceivingScreenShare({
        receiving: true
      }));

      var remoteElement = this.getScreenShareElementFunc();

      this.session.subscribe(stream,
        remoteElement, this._getCopyPublisherConfig());
    },

    /**
     * Handles the event when the remote stream is created.
     *
     * @param {StreamEvent} event The event details:
     * https://tokbox.com/opentok/libraries/client/js/reference/StreamEvent.html
     */
    _onRemoteStreamCreated: function(event) {
      if (event.stream[STREAM_PROPERTIES.HAS_VIDEO]) {
        this.dispatcher.dispatch(new sharedActions.VideoDimensionsChanged({
          isLocal: false,
          videoType: event.stream.videoType,
          dimensions: event.stream[STREAM_PROPERTIES.VIDEO_DIMENSIONS]
        }));
      }

      if (event.stream.videoType === "screen") {
        this._handleRemoteScreenShareCreated(event.stream);
        return;
      }

      var remoteElement = this.getRemoteElement();

      this.session.subscribe(event.stream,
        remoteElement, this._getCopyPublisherConfig());

      this._subscribedRemoteStream = true;
      if (this._checkAllStreamsConnected()) {
        this.dispatcher.dispatch(new sharedActions.MediaConnected());
      }
    },

    /**
     * Handles the event when the local stream is created.
     *
     * @param {StreamEvent} event The event details:
     * https://tokbox.com/opentok/libraries/client/js/reference/StreamEvent.html
     */
    _onLocalStreamCreated: function(event) {
      if (event.stream[STREAM_PROPERTIES.HAS_VIDEO]) {
        this.dispatcher.dispatch(new sharedActions.VideoDimensionsChanged({
          isLocal: true,
          videoType: event.stream.videoType,
          dimensions: event.stream[STREAM_PROPERTIES.VIDEO_DIMENSIONS]
        }));
      }
    },


    /**
     * Handles the event when the remote stream is destroyed.
     *
     * @param {StreamEvent} event The event details:
     * https://tokbox.com/opentok/libraries/client/js/reference/StreamEvent.html
     */
    _onRemoteStreamDestroyed: function(event) {
      if (event.stream.videoType !== "screen") {
        return;
      }

      // All we need to do is notify the store we're no longer receiving,
      // the sdk should do the rest.
      this.dispatcher.dispatch(new sharedActions.ReceivingScreenShare({
        receiving: false
      }));
    },

    /**
     * Called from the sdk when the media access dialog is opened.
     * Prevents the default action, to prevent the SDK's "allow access"
     * dialog from being shown.
     *
     * @param {OT.Event} event
     */
    _onAccessDialogOpened: function(event) {
      event.preventDefault();
    },

    /**
     * Handles the publishing being complete.
     *
     * @param {OT.Event} event
     */
    _onPublishComplete: function(event) {
      event.preventDefault();
      this._publisherReady = true;

      this.dispatcher.dispatch(new sharedActions.GotMediaPermission());

      this._maybePublishLocalStream();
    },

    /**
     * Handles publishing of media being denied.
     *
     * @param {OT.Event} event
     */
    _onPublishDenied: function(event) {
      // This prevents the SDK's "access denied" dialog showing.
      event.preventDefault();

      this.dispatcher.dispatch(new sharedActions.ConnectionFailure({
        reason: FAILURE_DETAILS.MEDIA_DENIED
      }));
    },

    _onOTException: function(event) {
      if (event.code === OT.ExceptionCodes.UNABLE_TO_PUBLISH &&
          event.message === "GetUserMedia") {
        // We free up the publisher here in case the store wants to try
        // grabbing the media again.
        if (this.publisher) {
          this.publisher.off("accessAllowed accessDenied accessDialogOpened streamCreated");
          this.publisher.destroy();
          delete this.publisher;
        }
        this.dispatcher.dispatch(new sharedActions.ConnectionFailure({
          reason: FAILURE_DETAILS.UNABLE_TO_PUBLISH_MEDIA
        }));
      }
    },

    /**
     * Handles publishing of property changes to a stream.
     */
    _onStreamPropertyChanged: function(event) {
      if (event.changedProperty == STREAM_PROPERTIES.VIDEO_DIMENSIONS) {
        this.dispatcher.dispatch(new sharedActions.VideoDimensionsChanged({
          isLocal: event.stream.connection.id == this.session.connection.id,
          videoType: event.stream.videoType,
          dimensions: event.stream[STREAM_PROPERTIES.VIDEO_DIMENSIONS]
        }));
      }
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
    },

    /**
     * Called when a screenshare is complete, publishes it to the session.
     */
    _onScreenShareGranted: function() {
      this.session.publish(this.screenshare);
      this.dispatcher.dispatch(new sharedActions.ScreenSharingState({
        state: SCREEN_SHARE_STATES.ACTIVE
      }));
    },

    /**
     * Called when a screenshare is denied. Notifies the other stores.
     */
    _onScreenShareDenied: function() {
      this.dispatcher.dispatch(new sharedActions.ScreenSharingState({
        state: SCREEN_SHARE_STATES.INACTIVE
      }));
    }
  };

  return OTSdkDriver;

})();
