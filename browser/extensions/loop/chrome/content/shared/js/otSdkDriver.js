"use strict"; /* This Source Code Form is subject to the terms of the Mozilla Public
               * License, v. 2.0. If a copy of the MPL was not distributed with this file,
               * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.OTSdkDriver = function () {
  "use strict";

  var sharedActions = loop.shared.actions;
  var FAILURE_DETAILS = loop.shared.utils.FAILURE_DETAILS;
  var STREAM_PROPERTIES = loop.shared.utils.STREAM_PROPERTIES;
  var SCREEN_SHARE_STATES = loop.shared.utils.SCREEN_SHARE_STATES;

  /**
   * This is a wrapper for the OT sdk. It is used to translate the SDK events into
   * actions, and instruct the SDK what to do as a result of actions.
   */
  var OTSdkDriver = function OTSdkDriver(options) {
    if (!options.constants) {
      throw new Error("Missing option constants");}

    if (!options.dispatcher) {
      throw new Error("Missing option dispatcher");}

    if (!options.sdk) {
      throw new Error("Missing option sdk");}


    this.dispatcher = options.dispatcher;
    this.sdk = options.sdk;

    this._constants = options.constants;
    this._useDataChannels = !!options.useDataChannels;
    this._isDesktop = !!options.isDesktop;

    this.connections = {};

    // Setup the metrics object to keep track of the number of connections we have
    // and the amount of streams.
    this._resetMetrics();

    this.dispatcher.register(this, [
    "setupStreamElements", 
    "setMute"]);


    // Set loop.debug.sdk to true in the browser, or in standalone:
    // localStorage.setItem("debug.sdk", true);
    loop.shared.utils.getBoolPreference("debug.sdk", function (enabled) {
      // We don't bother with the else case - as we only create one instance of
      // OTSdkDriver per window, and hence, we leave the sdk set to its default
      // value.
      if (enabled) {
        this.sdk.setLogLevel(this.sdk.DEBUG);}}.

    bind(this));

    /**
     * XXX This is a workaround for desktop machines that do not have a
     * camera installed. The SDK doesn't currently do use the new device
     * enumeration apis, when it does (bug 1138851), we can drop this part.
     */
    if (this._isDesktop) {
      // If there's no getSources function, the sdk defines its own and caches
      // the result. So here we define our own one which wraps around the
      // real device enumeration api.
      window.MediaStreamTrack.getSources = function (callback) {
        navigator.mediaDevices.enumerateDevices().then(function (devices) {
          var result = [];
          devices.forEach(function (device) {
            if (device.kind === "audioinput") {
              result.push({ kind: "audio" });}

            if (device.kind === "videoinput") {
              result.push({ kind: "video" });}});


          callback(result);});};}};





  OTSdkDriver.prototype = { 
    /**
     * Clones the publisher config into a new object, as the sdk modifies the
     * properties object.
     */
    get _getCopyPublisherConfig() {
      return _.extend({}, this.publisherConfig);}, 


    /**
     * Returns the required data channel settings.
     */
    get _getDataChannelSettings() {
      return { 
        channels: { 
          // We use a single channel for text. To make things simpler, we
          // always send on the publisher channel, and receive on the subscriber
          // channel.
          text: {}, 
          cursor: { 
            reliable: true } } };}, 





    /**
     * Resets the metrics for the driver.
     */
    _resetMetrics: function _resetMetrics() {
      this._metrics = { 
        connections: 0, 
        sendStreams: 0, 
        recvStreams: 0 };}, 



    /**
     * Handles the setupStreamElements action. Saves the required data and
     * kicks off the initialising of the publisher.
     *
     * @param {sharedActions.SetupStreamElements} actionData The data associated
     *   with the action. See action.js.
     */
    setupStreamElements: function setupStreamElements(actionData) {
      this.publisherConfig = actionData.publisherConfig;

      this.sdk.on("exception", this._onOTException.bind(this));

      // We expect the local video to be muted automatically by the SDK. Hence
      // we don't mute it manually here.
      this._mockPublisherEl = document.createElement("div");

      // At this state we init the publisher, even though we might be waiting for
      // the initial connect of the session. This saves time when setting up
      // the media.
      this.publisher = this.sdk.initPublisher(this._mockPublisherEl, 
      _.extend(this._getDataChannelSettings, this._getCopyPublisherConfig), 
      this._onPublishComplete.bind(this));

      this.publisher.on("streamCreated", this._onLocalStreamCreated.bind(this));
      this.publisher.on("streamDestroyed", this._onLocalStreamDestroyed.bind(this));
      this.publisher.on("accessAllowed", this._onPublishAllowed.bind(this));
      this.publisher.on("accessDenied", this._onPublishDenied.bind(this));
      this.publisher.on("accessDialogOpened", 
      this._onAccessDialogOpened.bind(this));}, 


    /**
     * Handles the setMute action. Informs the published stream to mute
     * or unmute audio as appropriate.
     *
     * @param {sharedActions.SetMute} actionData The data associated with the
     *                                           action. See action.js.
     */
    setMute: function setMute(actionData) {
      if (actionData.type === "audio") {
        this.publisher.publishAudio(actionData.enabled);} else 
      {
        this.publisher.publishVideo(actionData.enabled);}}, 



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
    startScreenShare: function startScreenShare(options) {
      // For browser sharing, we store the window Id so that we can avoid unnecessary
      // re-triggers.
      if (options.videoSource === "browser") {
        this._windowId = options.constraints.browserWindow;}


      var config = _.extend(this._getCopyPublisherConfig, options);

      this._mockScreenSharePreviewEl = document.createElement("div");

      this.screenshare = this.sdk.initPublisher(this._mockScreenSharePreviewEl, 
      config, this._onScreenSharePublishComplete.bind(this));
      this.screenshare.on("accessAllowed", this._onScreenShareGranted.bind(this));
      this.screenshare.on("accessDenied", this._onScreenSharePublishError.bind(this));
      this.screenshare.on("streamCreated", this._onScreenShareStreamCreated.bind(this));}, 


    /**
     * Initiates switching the browser window that is being shared.
     *
     * @param {Integer} windowId  The windowId of the browser.
     */
    switchAcquiredWindow: function switchAcquiredWindow(windowId) {
      if (windowId === this._windowId) {
        return;}


      this._windowId = windowId;
      this.screenshare._.switchAcquiredWindow(windowId);}, 


    /**
     * Ends an active screenshare session. Return `true` when an active screen-
     * sharing session was ended or `false` when no session is active.
     *
     * @returns {Boolean}
     */
    endScreenShare: function endScreenShare() {
      if (!this.screenshare) {
        return false;}


      this._notifyMetricsEvent("Publisher.streamDestroyed");

      this.session.unpublish(this.screenshare);
      this.screenshare.off("accessAllowed accessDenied streamCreated");
      this.screenshare.destroy();
      delete this.screenshare;
      delete this._mockScreenSharePreviewEl;
      delete this._windowId;
      return true;}, 


    /**
     * Paused or resumes an active screenshare session as appropriate.
     *
     * @param {Boolean} enabled  True if browser sharing should be enabled.
     */
    toggleBrowserSharing: function toggleBrowserSharing(enabled) {
      if (this.screenshare) {
        this.screenshare.publishVideo(enabled);}}, 



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
    connectSession: function connectSession(sessionData) {
      this.session = this.sdk.initSession(sessionData.sessionId);

      this.session.on("sessionDisconnected", 
      this._onSessionDisconnected.bind(this));
      this.session.on("connectionCreated", this._onConnectionCreated.bind(this));
      this.session.on("connectionDestroyed", 
      this._onConnectionDestroyed.bind(this));
      this.session.on("streamCreated", this._onRemoteStreamCreated.bind(this));
      this.session.on("streamDestroyed", this._onRemoteStreamDestroyed.bind(this));
      this.session.on("streamPropertyChanged", this._onStreamPropertyChanged.bind(this));
      this.session.on("signal:readyForDataChannel", this._onReadyForDataChannel.bind(this));

      // This starts the actual session connection.
      this.session.connect(sessionData.apiKey, sessionData.sessionToken, 
      this._onSessionConnectionCompleted.bind(this));}, 


    /**
     * Disconnects the sdk session.
     */
    disconnectSession: function disconnectSession() {
      this.endScreenShare();

      this.dispatcher.dispatch(new sharedActions.DataChannelsAvailable({ 
        available: false }));

      this.dispatcher.dispatch(new sharedActions.MediaStreamDestroyed({ 
        isLocal: true }));

      this.dispatcher.dispatch(new sharedActions.MediaStreamDestroyed({ 
        isLocal: false }));


      if (this.session) {
        this.session.off("sessionDisconnected streamCreated streamDestroyed " + 
        "connectionCreated connectionDestroyed " + 
        "streamPropertyChanged signal:readyForDataChannel");
        this.session.disconnect();
        delete this.session;

        this._notifyMetricsEvent("Session.connectionDestroyed", "local");}

      if (this.publisher) {
        this.publisher.off("accessAllowed accessDenied accessDialogOpened " + 
        "streamCreated streamDestroyed");
        this.publisher.destroy();
        delete this.publisher;}


      // Now reset the metrics as well.
      this._resetMetrics();

      // Also, tidy these variables ready for next time.
      delete this._sessionConnected;
      delete this._publisherReady;
      delete this._publishedLocalStream;
      delete this._subscribedRemoteStream;
      delete this._mockPublisherEl;
      delete this._publisherChannel;
      delete this._subscriberChannel;
      delete this._screenSubscriber;
      delete this._subscriber;
      this.connections = {};}, 


    /**
     * Oust all users from an ongoing session. This is typically done when a room
     * owner deletes the room.
     *
     * @param {Function} callback Function to be invoked once all connections are
     *                            ousted
     */
    forceDisconnectAll: function forceDisconnectAll(callback) {
      if (!this._sessionConnected) {
        callback();
        return;}


      var connectionNames = Object.keys(this.connections);
      if (connectionNames.length === 0) {
        callback();
        return;}

      var disconnectCount = 0;
      connectionNames.forEach(function (id) {
        var connection = this.connections[id];
        this.session.forceDisconnect(connection, function () {
          // When all connections have disconnected, call the callback, since
          // we're done.
          if (++disconnectCount === connectionNames.length) {
            callback();}});}, 


      this);}, 


    /**
     * Called once the session has finished connecting.
     *
     * @param {Error} error An OT error object, null if there was no error.
     */
    _onSessionConnectionCompleted: function _onSessionConnectionCompleted(error) {
      if (error) {
        console.error("Failed to complete connection", error);
        // We log this here before the connection failure to ensure the metrics
        // event gets to the server before the leave action occurs. Otherwise
        // the server won't log the metrics event because the user is no longer
        // in the room.
        this._notifyMetricsEvent("sdk.exception." + error.code);
        this.dispatcher.dispatch(new sharedActions.ConnectionFailure({ 
          reason: FAILURE_DETAILS.COULD_NOT_CONNECT }));

        return;}


      this.dispatcher.dispatch(new sharedActions.ConnectedToSdkServers());
      this._sessionConnected = true;
      this._maybePublishLocalStream();}, 


    /**
     * Handles the connection event for a peer's connection being dropped.
     *
     * @param {ConnectionEvent} event The event details
     * https://tokbox.com/opentok/libraries/client/js/reference/ConnectionEvent.html
     */
    _onConnectionDestroyed: function _onConnectionDestroyed(event) {
      var connection = event.connection;
      if (connection && connection.id in this.connections) {
        delete this.connections[connection.id];}


      this._notifyMetricsEvent("Session.connectionDestroyed", "peer");

      this.dispatcher.dispatch(new sharedActions.RemotePeerDisconnected({ 
        peerHungup: event.reason === "clientDisconnected" }));}, 



    /**
     * Handles the session event for the connection for this client being
     * destroyed.
     *
     * @param {SessionDisconnectEvent} event The event details:
     * https://tokbox.com/opentok/libraries/client/js/reference/SessionDisconnectEvent.html
     */
    _onSessionDisconnected: function _onSessionDisconnected(event) {
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
          return;}


      this._notifyMetricsEvent("Session." + event.reason);
      this.dispatcher.dispatch(new sharedActions.ConnectionFailure({ 
        reason: reason }));}, 



    /**
     * Handles the connection event for a newly connecting peer.
     *
     * @param {ConnectionEvent} event The event details
     * https://tokbox.com/opentok/libraries/client/js/reference/ConnectionEvent.html
     */
    _onConnectionCreated: function _onConnectionCreated(event) {
      var connection = event.connection;
      if (this.session.connection.id === connection.id) {
        // This is the connection event for our connection.
        this._notifyMetricsEvent("Session.connectionCreated", "local");
        return;}

      this.connections[connection.id] = connection;
      this._notifyMetricsEvent("Session.connectionCreated", "peer");
      this.dispatcher.dispatch(new sharedActions.RemotePeerConnected());}, 


    /**
     * Works out the current connection state based on the streams being
     * sent or received.
     */
    _getConnectionState: function _getConnectionState() {
      if (this._metrics.sendStreams) {
        return this._metrics.recvStreams ? "sendrecv" : "sending";}


      if (this._metrics.recvStreams) {
        return "receiving";}


      return "starting";}, 


    /**
     * Notifies of a metrics related event for tracking call setup purposes.
     * See https://wiki.mozilla.org/Loop/Architecture/Rooms#Updating_Session_State
     *
     * @param {String} eventName  The name of the event for the update.
     * @param {String} clientType Used for connection created/destoryed. Indicates
     *                            if it is for the "peer" or the "local" client.
     */
    _notifyMetricsEvent: function _notifyMetricsEvent(eventName, clientType) {
      if (!eventName) {
        return;}


      var state;

      // We intentionally don't bounds check these, in case there's an error
      // somewhere, if there is, we'll see it in the server metrics and are more
      // likely to investigate.
      switch (eventName) {
        case "Session.connectionCreated":
          this._metrics.connections++;
          if (clientType === "local") {
            state = "waiting";}

          break;
        case "Session.connectionDestroyed":
          this._metrics.connections--;
          if (clientType === "local") {
            // Don't log this, as the server doesn't accept it after
            // the room has been left.
            return;} else 
          if (!this._metrics.connections) {
            state = "waiting";}

          break;
        case "Publisher.streamCreated":
          this._metrics.sendStreams++;
          break;
        case "Publisher.streamDestroyed":
          this._metrics.sendStreams--;
          break;
        case "Session.av.streamCreated":
        case "Session.screen.streamCreated":
          this._metrics.recvStreams++;
          break;
        case "Session.streamDestroyed":
          this._metrics.recvStreams--;
          break;
        case "Session.networkDisconnected":
        case "Session.forceDisconnected":
        case "Session.subscribeCompleted":
        case "Session.screen.subscribeCompleted":
          break;
        default:
          // We don't want unexpected events being sent to the server, so
          // filter out the unexpected, and let the known ones through.
          if (!/^sdk\.(exception|datachannel)/.test(eventName)) {
            console.error("Unexpected event name", eventName);
            return;}}


      if (!state) {
        state = this._getConnectionState();}


      this.dispatcher.dispatch(new sharedActions.ConnectionStatus({ 
        event: eventName, 
        state: state, 
        connections: this._metrics.connections, 
        sendStreams: this._metrics.sendStreams, 
        recvStreams: this._metrics.recvStreams }));}, 



    /**
     * Handles when a remote screen share is created, subscribing to
     * the stream, and notifying the stores that a share is being
     * received.
     *
     * @param {Stream} stream The SDK Stream:
     * https://tokbox.com/opentok/libraries/client/js/reference/Stream.html
     */
    _handleRemoteScreenShareCreated: function _handleRemoteScreenShareCreated(stream) {
      // Let the stores know first if the screen sharing is paused or not so
      // they can update the display properly
      this.dispatcher.dispatch(new sharedActions.VideoScreenStreamChanged({ 
        hasVideo: stream[STREAM_PROPERTIES.HAS_VIDEO] }));


      // Let the stores know so they can update the display if needed.
      this.dispatcher.dispatch(new sharedActions.ReceivingScreenShare({ 
        receiving: true }));


      // There's no audio for screen shares so we don't need to worry about mute.
      this._mockScreenShareEl = document.createElement("div");
      this._screenSubscriber = this.session.subscribe(stream, this._mockScreenShareEl, 
      this._getCopyPublisherConfig, 
      this._onScreenShareSubscribeCompleted.bind(this));}, 


    /**
     * Handles the event when the remote stream is created.
     *
     * @param {StreamEvent} event The event details:
     * https://tokbox.com/opentok/libraries/client/js/reference/StreamEvent.html
     */
    _onRemoteStreamCreated: function _onRemoteStreamCreated(event) {
      if (event.stream[STREAM_PROPERTIES.HAS_VIDEO]) {
        this.dispatcher.dispatch(new sharedActions.VideoDimensionsChanged({ 
          isLocal: false, 
          videoType: event.stream.videoType, 
          dimensions: event.stream[STREAM_PROPERTIES.VIDEO_DIMENSIONS] }));}



      if (event.stream.videoType === "screen") {
        this._notifyMetricsEvent("Session.screen.streamCreated");
        this._handleRemoteScreenShareCreated(event.stream);
        return;}


      // Setting up the subscribe might want to be before the VideoDimensionsChange
      // dispatch. If so, we might also want to consider moving the dispatch to
      // _onSubscribeCompleted. However, this seems to work fine at the moment,
      // so we haven't felt the need to move it.

      // XXX This mock element currently handles playing audio for the session.
      // We might want to consider making the react tree responsible for playing
      // the audio, so that the incoming audio could be disable/tracked easly from
      // the UI (bug 1171896).
      this._mockSubscribeEl = document.createElement("div");

      this._notifyMetricsEvent("Session.av.streamCreated");

      this._subscriber = this.session.subscribe(event.stream, 
      this._mockSubscribeEl, this._getCopyPublisherConfig, 
      this._onSubscribeCompleted.bind(this));}, 


    /**
     * This method is passed as the "completionHandler" parameter to the SDK's
     * Session.subscribe.
     *
     * @param err {(null|Error)} - null on success, an Error object otherwise
     * @param sdkSubscriberObject {OT.Subscriber} - undocumented; returned on success
     * @param subscriberVideo {HTMLVideoElement} - used for unit testing
     */
    _onSubscribeCompleted: function _onSubscribeCompleted(err, sdkSubscriberObject, subscriberVideo) {
      // XXX test for and handle errors better (bug 1172140)
      if (err) {
        console.log("subscribe error:", err);
        return;}


      var sdkSubscriberVideo = subscriberVideo ? subscriberVideo : 
      this._mockSubscribeEl.querySelector("video");
      if (!sdkSubscriberVideo) {
        console.error("sdkSubscriberVideo unexpectedly falsy!");}


      sdkSubscriberObject.on("videoEnabled", this._onVideoEnabled.bind(this));
      sdkSubscriberObject.on("videoDisabled", this._onVideoDisabled.bind(this));

      this.dispatcher.dispatch(new sharedActions.MediaStreamCreated({ 
        hasAudio: sdkSubscriberObject.stream[STREAM_PROPERTIES.HAS_AUDIO], 
        hasVideo: sdkSubscriberObject.stream[STREAM_PROPERTIES.HAS_VIDEO], 
        isLocal: false, 
        srcMediaElement: sdkSubscriberVideo }));


      this._notifyMetricsEvent("Session.subscribeCompleted");
      this._subscribedRemoteStream = true;
      if (this._checkAllStreamsConnected()) {
        this.dispatcher.dispatch(new sharedActions.MediaConnected());}


      this._setupDataChannelIfNeeded(sdkSubscriberObject);}, 


    /**
     * This method is passed as the "completionHandler" parameter to the SDK's
     * Session.subscribe.
     *
     * @param err {(null|Error)} - null on success, an Error object otherwise
     * @param sdkSubscriberObject {OT.Subscriber} - undocumented; returned on success
     * @param subscriberVideo {HTMLVideoElement} - used for unit testing
     */
    _onScreenShareSubscribeCompleted: function _onScreenShareSubscribeCompleted(err, sdkSubscriberObject, subscriberVideo) {
      // XXX test for and handle errors better
      if (err) {
        console.log("subscribe error:", err);
        return;}


      var sdkSubscriberVideo = subscriberVideo ? subscriberVideo : 
      this._mockScreenShareEl.querySelector("video");

      // XXX no idea why this is necessary in addition to the dispatch in
      // _handleRemoteScreenShareCreated.  Maybe these should be separate
      // actions.  But even so, this shouldn't be necessary....
      this.dispatcher.dispatch(new sharedActions.ReceivingScreenShare({ 
        receiving: true, srcMediaElement: sdkSubscriberVideo }));


      this._notifyMetricsEvent("Session.screen.subscribeCompleted");}, 


    /**
     * Once a remote stream has been subscribed to, this triggers the data
     * channel set-up routines. A data channel cannot be requested before this
     * time as the peer connection is not set up.
     *
     * @param {OT.Subscriber} sdkSubscriberObject The subscriber object for the stream.
     *
     */
    _setupDataChannelIfNeeded: function _setupDataChannelIfNeeded(sdkSubscriberObject) {
      if (!this._useDataChannels) {
        return;}


      this.session.signal({ 
        type: "readyForDataChannel", 
        to: sdkSubscriberObject.stream.connection }, 
      function (signalError) {
        if (signalError) {
          console.error(signalError);}});



      // Set up data channels with a given type and message/channel handlers.
      var dataChannels = [
      ["text", 
      function (message) {
        // Append the timestamp. This is the time that gets shown.
        message.receivedTimestamp = new Date().toISOString();
        this.dispatcher.dispatch(new sharedActions.ReceivedTextChatMessage(message));}.
      bind(this), 
      function (channel) {
        this._subscriberChannel = channel;
        this._checkDataChannelsAvailable();}.
      bind(this)], 
      ["cursor", 
      function (message) {
        this.dispatcher.dispatch(new sharedActions.ReceivedCursorData(message));}.
      bind(this), 
      function (channel) {
        this._subscriberCursorChannel = channel;}.
      bind(this)]];


      dataChannels.forEach(function (args) {
        var type = args[0], 
        onMessage = args[1], 
        onChannel = args[2];
        sdkSubscriberObject._.getDataChannel(type, {}, function (err, channel) {
          // Sends will queue until the channel is fully open.
          if (err) {
            console.error(err);
            this._notifyMetricsEvent("sdk.datachannel.sub." + type + "." + err.message);
            return;}


          channel.on({ 
            message: function message(ev) {
              try {
                var message = JSON.parse(ev.data);
                onMessage(message);} 
              catch (ex) {
                console.error("Failed to process incoming chat message", ex);}}, 



            close: function close() {
              // XXX We probably want to dispatch and handle this somehow.
              console.log("Subscribed " + type + " data channel closed!");} });


          onChannel(channel);}.
        bind(this));}.
      bind(this));}, 


    /**
     * Handles receiving the signal that the other end of the connection
     * has subscribed to the stream and we're ready to setup the data channel.
     *
     * We create the publisher data channel when we get the signal as it means
     * that the remote client is setup for data
     * channels. Getting the data channel for the subscriber is handled
     * separately when the subscription completes.
     */
    _onReadyForDataChannel: function _onReadyForDataChannel() {
      // If we don't want data channels, just ignore the message. We haven't
      // send the other side a message, so it won't display anything.
      if (!this._useDataChannels) {
        return;}


      // Set up data channels with a given type and channel handler.
      var dataChannels = [
      ["text", 
      function (channel) {
        this._publisherChannel = channel;
        this._checkDataChannelsAvailable();}.
      bind(this)], 
      ["cursor", 
      function (channel) {
        this._publisherCursorChannel = channel;}.
      bind(this)]];


      // This won't work until a subscriber exists for this publisher
      dataChannels.forEach(function (args) {
        var type = args[0], 
        onChannel = args[1];
        this.publisher._.getDataChannel(type, {}, function (err, channel) {
          if (err) {
            console.error(err);
            this._notifyMetricsEvent("sdk.datachannel.pub." + type + "." + err.message);
            return;}


          channel.on({ 
            close: function close() {
              // XXX We probably want to dispatch and handle this somehow.
              console.log("Published " + type + " data channel closed!");} });


          onChannel(channel);}.
        bind(this));}.
      bind(this));}, 


    /**
     * Checks to see if all channels have been obtained, and if so it dispatches
     * a notification to the stores to inform them.
     */
    _checkDataChannelsAvailable: function _checkDataChannelsAvailable() {
      if (this._publisherChannel && this._subscriberChannel) {
        this.dispatcher.dispatch(new sharedActions.DataChannelsAvailable({ 
          available: true }));}}, 




    /**
     * Sends a text chat message on the data channel.
     *
     * @param {String} message The message to send.
     */
    sendTextChatMessage: function sendTextChatMessage(message) {
      this._publisherChannel.send(JSON.stringify(message));}, 


    /**
     * Sends the cursor events through the data channel.
     *
     * @param {String} message The message to send.
     */
    sendCursorMessage: function sendCursorMessage(message) {
      if (!this._publisherCursorChannel || !this._subscriberCursorChannel) {
        return;}


      message.userID = this.session.sessionId;
      this._publisherCursorChannel.send(JSON.stringify(message));}, 


    /**
     * Handles the event when the local stream is created.
     *
     * @param {StreamEvent} event The event details:
     * https://tokbox.com/opentok/libraries/client/js/reference/StreamEvent.html
     */
    _onLocalStreamCreated: function _onLocalStreamCreated(event) {
      this._notifyMetricsEvent("Publisher.streamCreated");

      var sdkLocalVideo = this._mockPublisherEl.querySelector("video");
      var hasVideo = event.stream[STREAM_PROPERTIES.HAS_VIDEO];
      var hasAudio = event.stream[STREAM_PROPERTIES.HAS_AUDIO];

      this.dispatcher.dispatch(new sharedActions.MediaStreamCreated({ 
        hasAudio: hasAudio, 
        hasVideo: hasVideo, 
        isLocal: true, 
        srcMediaElement: sdkLocalVideo }));


      // Only dispatch the video dimensions if we actually have video.
      if (hasVideo) {
        this.dispatcher.dispatch(new sharedActions.VideoDimensionsChanged({ 
          isLocal: true, 
          videoType: event.stream.videoType, 
          dimensions: event.stream[STREAM_PROPERTIES.VIDEO_DIMENSIONS] }));}}, 




    /**
     * Handles the event when the remote stream is destroyed.
     *
     * @param {StreamEvent} event The event details:
     * https://tokbox.com/opentok/libraries/client/js/reference/StreamEvent.html
     */
    _onRemoteStreamDestroyed: function _onRemoteStreamDestroyed(event) {
      this._notifyMetricsEvent("Session.streamDestroyed");

      if (event.stream.videoType !== "screen") {
        this.dispatcher.dispatch(new sharedActions.DataChannelsAvailable({ 
          available: false }));

        this.dispatcher.dispatch(new sharedActions.MediaStreamDestroyed({ 
          isLocal: false }));

        delete this._subscriberChannel;
        delete this._mockSubscribeEl;
        delete this._subscriber;
        return;}


      // All we need to do is notify the store we're no longer receiving,
      // the sdk should do the rest.
      this.dispatcher.dispatch(new sharedActions.ReceivingScreenShare({ 
        receiving: false }));

      delete this._mockScreenShareEl;
      delete this._screenSubscriber;}, 


    /**
     * Handles the event when the remote stream is destroyed.
     */
    _onLocalStreamDestroyed: function _onLocalStreamDestroyed() {
      this._notifyMetricsEvent("Publisher.streamDestroyed");
      this.dispatcher.dispatch(new sharedActions.DataChannelsAvailable({ 
        available: false }));

      this.dispatcher.dispatch(new sharedActions.MediaStreamDestroyed({ 
        isLocal: true }));

      delete this._publisherChannel;
      delete this._mockPublisherEl;}, 


    /**
     * Called from the sdk when the media access dialog is opened.
     * Prevents the default action, to prevent the SDK's "allow access"
     * dialog from being shown.
     *
     * @param {OT.Event} event
     */
    _onAccessDialogOpened: function _onAccessDialogOpened(event) {
      event.preventDefault();}, 


    /**
     * Handles the publishing being complete.
     *
     * @param {OT.Event} event
     */
    _onPublishAllowed: function _onPublishAllowed(event) {
      event.preventDefault();
      this._publisherReady = true;

      this.dispatcher.dispatch(new sharedActions.GotMediaPermission());

      this._maybePublishLocalStream();}, 


    /**
     *
     * Handles publisher Complete.
     *
     * @param {Error} error An OT error object, null if there was no error.
     */
    _onPublishComplete: function _onPublishComplete(error) {
      if (!error) {
        // Nothing to do for the success case.
        return;}


      if (error.message && error.message === "DENIED") {
        // In the DENIED case, this will be handled by _onPublishDenied.
        return;}


      if (!this.publisher) {
        return;}


      // We free up the publisher here in case the store wants to try
      // grabbing the media again.
      this.publisher.off("accessAllowed accessDenied accessDialogOpened streamCreated");
      this.publisher.destroy();
      delete this.publisher;
      delete this._mockPublisherEl;

      this.dispatcher.dispatch(new sharedActions.ConnectionFailure({ 
        reason: FAILURE_DETAILS.UNABLE_TO_PUBLISH_MEDIA }));


      // Exceptions are logged via the _onOTException handler.
    }, 

    /**
     * Handles publishing of media being denied.
     *
     * @param {OT.Event} event
     */
    _onPublishDenied: function _onPublishDenied(event) {
      // This prevents the SDK's "access denied" dialog showing.
      event.preventDefault();

      this.dispatcher.dispatch(new sharedActions.ConnectionFailure({ 
        reason: FAILURE_DETAILS.MEDIA_DENIED }));


      delete this._mockPublisherEl;}, 


    /**
     * Handles exceptions being raised by the OT SDK.
     *
     * @param  {OT.Event} event
     */
    _onOTException: function _onOTException(event) {
      var baseException = "sdk.exception.";
      if (event.target && 
      event.target === this.screenshare || 
      event.target === this._screenSubscriber) {
        baseException += "screen.";}


      switch (event.code) {
        case OT.ExceptionCodes.PUBLISHER_ICE_WORKFLOW_FAILED:
        case OT.ExceptionCodes.SUBSCRIBER_ICE_WORKFLOW_FAILED:
          this.dispatcher.dispatch(new sharedActions.ConnectionFailure({ 
            reason: FAILURE_DETAILS.ICE_FAILED }));

          this._notifyMetricsEvent(baseException + event.code + "." + event.message);
          break;
        case OT.ExceptionCodes.TERMS_OF_SERVICE_FAILURE:
          this.dispatcher.dispatch(new sharedActions.ConnectionFailure({ 
            reason: FAILURE_DETAILS.TOS_FAILURE }));

          // We still need to log the exception so that the server knows why this
          // attempt failed.
          this._notifyMetricsEvent(baseException + event.code);
          break;
        case OT.ExceptionCodes.UNABLE_TO_PUBLISH:
          // Don't report errors for GetUserMedia events as these are expected if
          // the user denies the prompt.
          if (event.message !== "GetUserMedia") {
            this._notifyMetricsEvent(baseException + event.code + "." + event.message);}

          break;
        default:
          this._notifyMetricsEvent(baseException + event.code + "." + event.message);
          break;}}, 



    /**
     * Handles publishing of property changes to a stream.
     */
    _onStreamPropertyChanged: function _onStreamPropertyChanged(event) {
      switch (event.changedProperty) {
        case STREAM_PROPERTIES.VIDEO_DIMENSIONS:
          this.dispatcher.dispatch(new sharedActions.VideoDimensionsChanged({ 
            isLocal: event.stream.connection.id === this.session.connection.id, 
            videoType: event.stream.videoType, 
            dimensions: event.stream[STREAM_PROPERTIES.VIDEO_DIMENSIONS] }));

          break;
        case STREAM_PROPERTIES.HAS_VIDEO:
          if (event.stream.videoType === "screen") {
            this.dispatcher.dispatch(new sharedActions.VideoScreenStreamChanged({ 
              hasVideo: event.newValue }));}


          break;}}, 



    /**
     * Handle the (remote) VideoEnabled event from the subscriber object
     * by dispatching an action with the (hidden) video element from
     * which to copy the stream when attaching it to visible video element
     * that the views control directly.
     *
     * @see https://tokbox.com/opentok/libraries/client/js/reference/VideoEnabledChangedEvent.html
     * @private
     */
    _onVideoEnabled: function _onVideoEnabled() {
      var sdkSubscriberVideo = this._mockSubscribeEl.querySelector("video");
      if (!sdkSubscriberVideo) {
        console.error("sdkSubscriberVideo unexpectedly falsy!");}


      this.dispatcher.dispatch(new sharedActions.RemoteVideoStatus({ 
        videoEnabled: true }));}, 



    /**
     * Handle the SDK disabling of remote video by dispatching the
     * appropriate event.
     *
     * @see https://tokbox.com/opentok/libraries/client/js/reference/VideoEnabledChangedEvent.html
     * @private
     */
    _onVideoDisabled: function _onVideoDisabled() {
      this.dispatcher.dispatch(new sharedActions.RemoteVideoStatus({ 
        videoEnabled: false }));}, 



    /**
     * Publishes the local stream if the session is connected
     * and the publisher is ready.
     */
    _maybePublishLocalStream: function _maybePublishLocalStream() {
      if (this._sessionConnected && this._publisherReady) {
        // We are clear to publish the stream to the session.
        this.session.publish(this.publisher);

        // Now record the fact, and check if we've got all media yet.
        this._publishedLocalStream = true;
        if (this._checkAllStreamsConnected()) {
          this.dispatcher.dispatch(new sharedActions.MediaConnected());}}}, 




    /**
     * Used to check if both local and remote streams are available
     * and send an action if they are.
     */
    _checkAllStreamsConnected: function _checkAllStreamsConnected() {
      return this._publishedLocalStream && 
      this._subscribedRemoteStream;}, 


    /**
     * Called when a screenshare is complete, publishes it to the session.
     */
    _onScreenShareGranted: function _onScreenShareGranted() {
      this.session.publish(this.screenshare);
      this.dispatcher.dispatch(new sharedActions.ScreenSharingState({ 
        state: SCREEN_SHARE_STATES.ACTIVE }));}, 



    /**
     * Called when a screenshare is complete.
     *
     * @param {Error} error An OT error object, null if there was no error.
     */
    _onScreenSharePublishComplete: function _onScreenSharePublishComplete(error) {
      if (!error) {
        // Nothing to do for the success case.
        return;}


      // Free up publisher
      this.screenshare.off("accessAllowed accessDenied streamCreated");
      this.screenshare.destroy();
      delete this.screenshare;
      delete this._mockScreenSharePreviewEl;
      this.dispatcher.dispatch(new sharedActions.ScreenSharingState({ 
        state: SCREEN_SHARE_STATES.INACTIVE }));


      // Exceptions are logged via the _onOTException handler.
    }, 

    /**
     * Called when a screenshare is denied. Notifies the other stores.
     */
    _onScreenSharePublishError: function _onScreenSharePublishError() {
      this.dispatcher.dispatch(new sharedActions.ScreenSharingState({ 
        state: SCREEN_SHARE_STATES.INACTIVE }));

      this.screenshare.off("accessAllowed accessDenied streamCreated");
      this.screenshare.destroy();
      delete this.screenshare;
      delete this._mockScreenSharePreviewEl;}, 


    /**
     * Called when a screenshare stream is published.
     */
    _onScreenShareStreamCreated: function _onScreenShareStreamCreated() {
      this._notifyMetricsEvent("Publisher.streamCreated");} };



  return OTSdkDriver;}();
