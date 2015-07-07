/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.store = loop.store || {};

(function() {
  var sharedActions = loop.shared.actions;
  var CALL_TYPES = loop.shared.utils.CALL_TYPES;
  var REST_ERRNOS = loop.shared.utils.REST_ERRNOS;
  var FAILURE_DETAILS = loop.shared.utils.FAILURE_DETAILS;

  /**
   * Websocket states taken from:
   * https://docs.services.mozilla.com/loop/apis.html#call-progress-state-change-progress
   */
  var WS_STATES = loop.store.WS_STATES = {
    // The call is starting, and the remote party is not yet being alerted.
    INIT: "init",
    // The called party is being alerted.
    ALERTING: "alerting",
    // The call is no longer being set up and has been aborted for some reason.
    TERMINATED: "terminated",
    // The called party has indicated that he has answered the call,
    // but the media is not yet confirmed.
    CONNECTING: "connecting",
    // One of the two parties has indicated successful media set up,
    // but the other has not yet.
    HALF_CONNECTED: "half-connected",
    // Both endpoints have reported successfully establishing media.
    CONNECTED: "connected"
  };

  var CALL_STATES = loop.store.CALL_STATES = {
    // The initial state of the view.
    INIT: "cs-init",
    // The store is gathering the call data from the server.
    GATHER: "cs-gather",
    // The initial data has been gathered, the websocket is connecting, or has
    // connected, and waiting for the other side to connect to the server.
    CONNECTING: "cs-connecting",
    // The websocket has received information that we're now alerting
    // the peer.
    ALERTING: "cs-alerting",
    // The call is ongoing.
    ONGOING: "cs-ongoing",
    // The call ended successfully.
    FINISHED: "cs-finished",
    // The user has finished with the window.
    CLOSE: "cs-close",
    // The call was terminated due to an issue during connection.
    TERMINATED: "cs-terminated"
  };

  /**
   * Conversation store.
   *
   * @param {loop.Dispatcher} dispatcher  The dispatcher for dispatching actions
   *                                      and registering to consume actions.
   * @param {Object} options Options object:
   * - {client}           client           The client object.
   * - {mozLoop}          mozLoop          The MozLoop API object.
   * - {loop.OTSdkDriver} loop.OTSdkDriver The SDK Driver
   */
  loop.store.ConversationStore = loop.store.createStore({
    // Further actions are registered in setupWindowData when
    // we know what window type this is.
    actions: [
      "setupWindowData"
    ],

    getInitialStoreState: function() {
      return {
        // The id of the window. Currently used for getting the window id.
        windowId: undefined,
        // The current state of the call
        callState: CALL_STATES.INIT,
        // The reason if a call was terminated
        callStateReason: undefined,
        // True if the call is outgoing, false if not, undefined if unknown
        outgoing: undefined,
        // The contact being called for outgoing calls
        contact: undefined,
        // The call type for the call.
        // XXX Don't hard-code, this comes from the data in bug 1072323
        callType: CALL_TYPES.AUDIO_VIDEO,
        // A link for emailing once obtained from the server
        emailLink: undefined,

        // Call Connection information
        // The call id from the loop-server
        callId: undefined,
        // The caller id of the contacting side
        callerId: undefined,
        // True if media has been connected both-ways.
        mediaConnected: false,
        // The connection progress url to connect the websocket
        progressURL: undefined,
        // The websocket token that allows connection to the progress url
        websocketToken: undefined,
        // SDK API key
        apiKey: undefined,
        // SDK session ID
        sessionId: undefined,
        // SDK session token
        sessionToken: undefined,
        // If the local audio is muted
        audioMuted: false,
        // If the local video is muted
        videoMuted: false,
        remoteVideoEnabled: false
      };
    },

    /**
     * Handles initialisation of the store.
     *
     * @param  {Object} options    Options object.
     */
    initialize: function(options) {
      options = options || {};

      if (!options.client) {
        throw new Error("Missing option client");
      }
      if (!options.sdkDriver) {
        throw new Error("Missing option sdkDriver");
      }
      if (!options.mozLoop) {
        throw new Error("Missing option mozLoop");
      }

      this.client = options.client;
      this.sdkDriver = options.sdkDriver;
      this.mozLoop = options.mozLoop;
      this._isDesktop = options.isDesktop || false;
    },

    /**
     * Handles the connection failure action, setting the state to
     * terminated.
     *
     * @param {sharedActions.ConnectionFailure} actionData The action data.
     */
    connectionFailure: function(actionData) {
      /**
       * XXX This is a workaround for desktop machines that do not have a
       * camera installed. As we don't yet have device enumeration, when
       * we do, this can be removed (bug 1138851), and the sdk should handle it.
       */
      if (this._isDesktop &&
          actionData.reason === FAILURE_DETAILS.UNABLE_TO_PUBLISH_MEDIA &&
          this.getStoreState().videoMuted === false) {
        // We failed to publish with media, so due to the bug, we try again without
        // video.
        this.setStoreState({videoMuted: true});
        this.sdkDriver.retryPublishWithoutVideo();
        return;
      }

      this._endSession();
      this.setStoreState({
        callState: CALL_STATES.TERMINATED,
        callStateReason: actionData.reason
      });
    },

    /**
     * Handles the connection progress action, setting the next state
     * appropriately.
     *
     * @param {sharedActions.ConnectionProgress} actionData The action data.
     */
    connectionProgress: function(actionData) {
      var state = this.getStoreState();

      switch(actionData.wsState) {
        case WS_STATES.INIT: {
          if (state.callState === CALL_STATES.GATHER) {
            this.setStoreState({callState: CALL_STATES.CONNECTING});
          }
          break;
        }
        case WS_STATES.ALERTING: {
          this.setStoreState({callState: CALL_STATES.ALERTING});
          break;
        }
        case WS_STATES.CONNECTING: {
          if (state.outgoing) {
            // We can start the connection right away if we're the outgoing
            // call because this is the only way we know the other peer has
            // accepted.
            this._startCallConnection();
          } else if (state.callState !== CALL_STATES.ONGOING) {
            console.error("Websocket unexpectedly changed to next state whilst waiting for call acceptance.");
            // Abort and close the window.
            this.declineCall(new sharedActions.DeclineCall({blockCaller: false}));
          }
          break;
        }
        case WS_STATES.HALF_CONNECTED:
        case WS_STATES.CONNECTED: {
          if (this.getStoreState("callState") !== CALL_STATES.ONGOING) {
            console.error("Unexpected websocket state received in wrong callState");
            this.setStoreState({callState: CALL_STATES.ONGOING});
          }
          break;
        }
        default: {
          console.error("Unexpected websocket state passed to connectionProgress:",
            actionData.wsState);
        }
      }
    },

    setupWindowData: function(actionData) {
      var windowType = actionData.type;
      if (windowType !== "outgoing" &&
          windowType !== "incoming") {
        // Not for this store, don't do anything.
        return;
      }

      this.dispatcher.register(this, [
        "connectionFailure",
        "connectionProgress",
        "acceptCall",
        "declineCall",
        "connectCall",
        "hangupCall",
        "remotePeerDisconnected",
        "cancelCall",
        "retryCall",
        "mediaConnected",
        "setMute",
        "fetchRoomEmailLink",
        "localVideoEnabled",
        "remoteVideoDisabled",
        "remoteVideoEnabled",
        "windowUnload"
      ]);

      this.setStoreState({
        apiKey: actionData.apiKey,
        callerId: actionData.callerId,
        callId: actionData.callId,
        callState: CALL_STATES.GATHER,
        callType: actionData.callType,
        contact: actionData.contact,
        outgoing: windowType === "outgoing",
        progressURL: actionData.progressURL,
        sessionId: actionData.sessionId,
        sessionToken: actionData.sessionToken,
        videoMuted: actionData.callType === CALL_TYPES.AUDIO_ONLY,
        websocketToken: actionData.websocketToken,
        windowId: actionData.windowId
      });

      if (this.getStoreState("outgoing")) {
        this._setupOutgoingCall();
      } else {
        this._setupIncomingCall();
      }
    },

    /**
     * Handles starting a call connection - connecting the session on the
     * sdk, and setting the state appropriately.
     */
    _startCallConnection: function() {
      var state = this.getStoreState();

      this.sdkDriver.connectSession({
        apiKey: state.apiKey,
        sessionId: state.sessionId,
        sessionToken: state.sessionToken,
        sendTwoWayMediaTelemetry: state.outgoing // only one side of the call
      });
      this.mozLoop.addConversationContext(
        state.windowId,
        state.sessionId,
        state.callId);
      this.setStoreState({callState: CALL_STATES.ONGOING});
    },

    /**
     * Accepts an incoming call.
     *
     * @param {sharedActions.AcceptCall} actionData
     */
    acceptCall: function(actionData) {
      if (this.getStoreState("outgoing")) {
        console.error("Received AcceptCall action in outgoing call state");
        return;
      }

      this.setStoreState({
        callType: actionData.callType,
        videoMuted: actionData.callType === CALL_TYPES.AUDIO_ONLY
      });

      this._websocket.accept();

      this._startCallConnection();
    },

    /**
     * Declines an incoming call.
     *
     * @param {sharedActions.DeclineCall} actionData
     */
    declineCall: function(actionData) {
      if (actionData.blockCaller) {
        this.mozLoop.calls.blockDirectCaller(this.getStoreState("callerId"),
          function(err) {
            // XXX The conversation window will be closed when this cb is triggered
            // figure out if there is a better way to report the error to the user
            // (bug 1103150).
            console.log(err.fileName + ":" + err.lineNumber + ": " + err.message);
          });
      }

      this._websocket.decline();

      // Now we've declined, end the session and close the window.
      this._endSession();
      this.setStoreState({callState: CALL_STATES.CLOSE});
    },

    /**
     * Handles the connect call action, this saves the appropriate
     * data and starts the connection for the websocket to notify the
     * server of progress.
     *
     * @param {sharedActions.ConnectCall} actionData The action data.
     */
    connectCall: function(actionData) {
      this.setStoreState(actionData.sessionData);
      this._connectWebSocket();
    },

    /**
     * Hangs up an ongoing call.
     */
    hangupCall: function() {
      if (this._websocket) {
        // Let the server know the user has hung up.
        this._websocket.mediaFail();
      }

      this._endSession();
      this.setStoreState({callState: CALL_STATES.FINISHED});
    },

    /**
     * The remote peer disconnected from the session.
     *
     * @param {sharedActions.RemotePeerDisconnected} actionData
     */
    remotePeerDisconnected: function(actionData) {
      this._endSession();

      // If the peer hungup, we end normally, otherwise
      // we treat this as a call failure.
      if (actionData.peerHungup) {
        this.setStoreState({callState: CALL_STATES.FINISHED});
      } else {
        this.setStoreState({
          callState: CALL_STATES.TERMINATED,
          callStateReason: "peerNetworkDisconnected"
        });
      }
    },

    /**
     * Cancels a call. This can happen for incoming or outgoing calls.
     * Although the user doesn't "cancel" an incoming call, it may be that
     * the remote peer cancelled theirs before the incoming call was accepted.
     */
    cancelCall: function() {
      if (this.getStoreState("outgoing")) {
        var callState = this.getStoreState("callState");
        if (this._websocket &&
            (callState === CALL_STATES.CONNECTING ||
             callState === CALL_STATES.ALERTING)) {
          // Let the server know the user has hung up.
          this._websocket.cancel();
        }
      }

      this._endSession();
      this.setStoreState({callState: CALL_STATES.CLOSE});
    },

    /**
     * Retries a call
     */
    retryCall: function() {
      var callState = this.getStoreState("callState");
      if (callState !== CALL_STATES.TERMINATED) {
        console.error("Unexpected retry in state", callState);
        return;
      }

      this.setStoreState({callState: CALL_STATES.GATHER});
      if (this.getStoreState("outgoing")) {
        this._setupOutgoingCall();
      }
    },

    /**
     * Notifies that all media is now connected
     */
    mediaConnected: function() {
      this._websocket.mediaUp();
      this.setStoreState({mediaConnected: true});
    },

    /**
     * Records the mute state for the stream.
     *
     * @param {sharedActions.setMute} actionData The mute state for the stream type.
     */
    setMute: function(actionData) {
      var newState = {};
      newState[actionData.type + "Muted"] = !actionData.enabled;
      this.setStoreState(newState);
    },

    /**
     * Fetches a new room URL intended to be sent over email when a contact
     * can't be reached.
     */
    fetchRoomEmailLink: function(actionData) {
      this.mozLoop.rooms.create({
        roomName: actionData.roomName,
        roomOwner: actionData.roomOwner,
        maxSize: loop.store.MAX_ROOM_CREATION_SIZE,
        expiresIn: loop.store.DEFAULT_EXPIRES_IN
      }, function(err, createdRoomData) {
        var buckets = this.mozLoop.ROOM_CREATE;
        if (err) {
          this.trigger("error:emailLink");
          this.mozLoop.telemetryAddValue("LOOP_ROOM_CREATE", buckets.CREATE_FAIL);
          return;
        }
        this.setStoreState({"emailLink": createdRoomData.roomUrl});
        this.mozLoop.telemetryAddValue("LOOP_ROOM_CREATE", buckets.CREATE_SUCCESS);
      }.bind(this));
    },

    /**
     * Handles when the remote stream has been enabled and is supplied.
     *
     * @param  {sharedActions.RemoteVideoEnabled} actionData
     */
    remoteVideoEnabled: function(actionData) {
      this.setStoreState({
        remoteVideoEnabled: true,
        remoteSrcVideoObject: actionData.srcVideoObject
      });
    },

    /**
     * Handles when the remote stream has been disabled, e.g. due to video mute.
     *
     * @param {sharedActions.RemoteVideoDisabled} actionData
     */
    remoteVideoDisabled: function(actionData) {
      this.setStoreState({
        remoteVideoEnabled: false,
        remoteSrcVideoObject: undefined});
    },

    /**
     * Handles when the local stream is supplied.
     *
     * XXX should write a localVideoDisabled action in otSdkDriver.js to
     * positively ensure proper cleanup (handled by window teardown currently)
     * (see bug 1171978)
     *
     * @param  {sharedActions.LocalVideoEnabled} actionData
     */
    localVideoEnabled: function(actionData) {
      this.setStoreState({
        localSrcVideoObject: actionData.srcVideoObject
      });
    },

    /**
     * Called when the window is unloaded, either by code, or by the user
     * explicitly closing it.  Expected to do any necessary housekeeping, such
     * as shutting down the call cleanly and adding any relevant telemetry data.
     */
    windowUnload: function() {
      if (!this.getStoreState("outgoing") &&
          this.getStoreState("callState") === CALL_STATES.ALERTING &&
          this._websocket) {
        this._websocket.decline();
      }

      this._endSession();
    },

    /**
     * Sets up an incoming call. All we really need to do here is
     * to connect the websocket, as we've already got all the information
     * when the window opened.
     */
    _setupIncomingCall: function() {
      this._connectWebSocket();
    },

    /**
     * Obtains the outgoing call data from the server and handles the
     * result.
     */
    _setupOutgoingCall: function() {
      var contactAddresses = [];
      var contact = this.getStoreState("contact");

      this.mozLoop.calls.setCallInProgress(this.getStoreState("windowId"));

      function appendContactValues(property, strip) {
        if (contact.hasOwnProperty(property)) {
          contact[property].forEach(function(item) {
            if (strip) {
              contactAddresses.push(item.value
                .replace(/^(\+)?(.*)$/g, function(m, prefix, number) {
                  return (prefix || "") + number.replace(/[\D]+/g, "");
                }));
            } else {
              contactAddresses.push(item.value);
            }
          });
        }
      }

      appendContactValues("email");
      appendContactValues("tel", true);

      this.client.setupOutgoingCall(contactAddresses,
        this.getStoreState("callType"),
        function(err, result) {
          if (err) {
            console.error("Failed to get outgoing call data", err);
            var failureReason = "setup";
            if (err.errno == REST_ERRNOS.USER_UNAVAILABLE) {
              failureReason = REST_ERRNOS.USER_UNAVAILABLE;
            }
            this.dispatcher.dispatch(
              new sharedActions.ConnectionFailure({reason: failureReason}));
            return;
          }

          // Success, dispatch a new action.
          this.dispatcher.dispatch(
            new sharedActions.ConnectCall({sessionData: result}));
        }.bind(this)
      );
    },

    /**
     * Sets up and connects the websocket to the server. The websocket
     * deals with sending and obtaining status via the server about the
     * setup of the call.
     */
    _connectWebSocket: function() {
      this._websocket = new loop.CallConnectionWebSocket({
        url: this.getStoreState("progressURL"),
        callId: this.getStoreState("callId"),
        websocketToken: this.getStoreState("websocketToken")
      });

      this._websocket.promiseConnect().then(
        function(progressState) {
          this.dispatcher.dispatch(new sharedActions.ConnectionProgress({
            // This is the websocket call state, i.e. waiting for the
            // other end to connect to the server.
            wsState: progressState
          }));
        }.bind(this),
        function(error) {
          console.error("Websocket failed to connect", error);
          this.dispatcher.dispatch(new sharedActions.ConnectionFailure({
            reason: "websocket-setup"
          }));
        }.bind(this)
      );

      this.listenTo(this._websocket, "progress", this._handleWebSocketProgress);
    },

    /**
     * Ensures the session is ended and the websocket is disconnected.
     */
    _endSession: function(nextState) {
      this.sdkDriver.disconnectSession();
      if (this._websocket) {
        this.stopListening(this._websocket);

        // Now close the websocket.
        this._websocket.close();
        delete this._websocket;
      }

      this.mozLoop.calls.clearCallInProgress(
        this.getStoreState("windowId"));
    },

    /**
     * If we hit any of the termination reasons, and the user hasn't accepted
     * then it seems reasonable to close the window/abort the incoming call.
     *
     * If the user has accepted the call, and something's happened, display
     * the call failed view.
     *
     * https://wiki.mozilla.org/Loop/Architecture/MVP#Termination_Reasons
     *
     * For outgoing calls, we treat all terminations as failures.
     *
     * @param {Object} progressData  The progress data received from the websocket.
     * @param {String} previousState The previous state the websocket was in.
     */
    _handleWebSocketStateTerminated: function(progressData, previousState) {
      if (this.getStoreState("outgoing") ||
          (previousState !== WS_STATES.INIT &&
           previousState !== WS_STATES.ALERTING)) {
        // For outgoing calls we can treat everything as connection failure.
        this.dispatcher.dispatch(new sharedActions.ConnectionFailure({
          reason: progressData.reason
        }));
        return;
      }

      this.dispatcher.dispatch(new sharedActions.CancelCall());
    },

    /**
     * Used to handle any progressed received from the websocket. This will
     * dispatch new actions so that the data can be handled appropriately.
     *
     * @param {Object} progressData  The progress data received from the websocket.
     * @param {String} previousState The previous state the websocket was in.
     */
    _handleWebSocketProgress: function(progressData, previousState) {
      switch(progressData.state) {
        case WS_STATES.TERMINATED: {
          this._handleWebSocketStateTerminated(progressData, previousState);
          break;
        }
        default: {
          this.dispatcher.dispatch(new sharedActions.ConnectionProgress({
            wsState: progressData.state
          }));
          break;
        }
      }
    }
  });
})();
