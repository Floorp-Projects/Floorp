/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.store = (function() {

  var sharedActions = loop.shared.actions;
  var sharedUtils = loop.shared.utils;

  var CALL_STATES = {
    // The initial state of the view.
    INIT: "init",
    // The store is gathering the call data from the server.
    GATHER: "gather",
    // The websocket has connected to the server and is waiting
    // for the other peer to connect to the websocket.
    CONNECTING: "connecting",
    // The websocket has received information that we're now alerting
    // the peer.
    ALERTING: "alerting",
    // The call was terminated due to an issue during connection.
    TERMINATED: "terminated"
  };


  var ConversationStore = Backbone.Model.extend({
    defaults: {
      // The current state of the call
      callState: CALL_STATES.INIT,
      // The reason if a call was terminated
      callStateReason: undefined,
      // The error information, if there was a failure
      error: undefined,
      // True if the call is outgoing, false if not, undefined if unknown
      outgoing: undefined,
      // The id of the person being called for outgoing calls
      calleeId: undefined,
      // The call type for the call.
      // XXX Don't hard-code, this comes from the data in bug 1072323
      callType: sharedUtils.CALL_TYPES.AUDIO_VIDEO,

      // Call Connection information
      // The call id from the loop-server
      callId: undefined,
      // The connection progress url to connect the websocket
      progressURL: undefined,
      // The websocket token that allows connection to the progress url
      websocketToken: undefined,
      // SDK API key
      apiKey: undefined,
      // SDK session ID
      sessionId: undefined,
      // SDK session token
      sessionToken: undefined
    },

    /**
     * Constructor
     *
     * Options:
     * - {loop.Dispatcher} dispatcher The dispatcher for dispatching actions and
     *                                registering to consume actions.
     * - {Object} client              A client object for communicating with the server.
     *
     * @param  {Object} attributes Attributes object.
     * @param  {Object} options    Options object.
     */
    initialize: function(attributes, options) {
      options = options || {};

      if (!options.dispatcher) {
        throw new Error("Missing option dispatcher");
      }
      if (!options.client) {
        throw new Error("Missing option client");
      }

      this.client = options.client;
      this.dispatcher = options.dispatcher;

      this.dispatcher.register(this, [
        "connectionFailure",
        "connectionProgress",
        "gatherCallData",
        "connectCall"
      ]);
    },

    /**
     * Handles the connection failure action, setting the state to
     * terminated.
     *
     * @param {sharedActions.ConnectionFailure} actionData The action data.
     */
    connectionFailure: function(actionData) {
      this.set({
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
      // XXX Turn this into a state machine?
      if (actionData.state === "alerting" &&
          (this.get("callState") === CALL_STATES.CONNECTING ||
           this.get("callState") === CALL_STATES.GATHER)) {
        this.set({
          callState: CALL_STATES.ALERTING
        });
      }
      if (actionData.state === "connecting" &&
          this.get("callState") === CALL_STATES.GATHER) {
        this.set({
          callState: CALL_STATES.CONNECTING
        });
      }
    },

    /**
     * Handles the gather call data action, setting the state
     * and starting to get the appropriate data for the type of call.
     *
     * @param {sharedActions.GatherCallData} actionData The action data.
     */
    gatherCallData: function(actionData) {
      this.set({
        calleeId: actionData.calleeId,
        outgoing: !!actionData.calleeId,
        callId: actionData.callId,
        callState: CALL_STATES.GATHER
      });

      if (this.get("outgoing")) {
        this._setupOutgoingCall();
      } // XXX Else, other types aren't supported yet.
    },

    /**
     * Handles the connect call action, this saves the appropriate
     * data and starts the connection for the websocket to notify the
     * server of progress.
     *
     * @param {sharedActions.ConnectCall} actionData The action data.
     */
    connectCall: function(actionData) {
      this.set(actionData.sessionData);
      this._connectWebSocket();
    },

    /**
     * Obtains the outgoing call data from the server and handles the
     * result.
     */
    _setupOutgoingCall: function() {
      // XXX For now, we only have one calleeId, so just wrap that in an array.
      this.client.setupOutgoingCall([this.get("calleeId")],
        this.get("callType"),
        function(err, result) {
          if (err) {
            console.error("Failed to get outgoing call data", err);
            this.dispatcher.dispatch(
              new sharedActions.ConnectionFailure({reason: "setup"}));
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
        url: this.get("progressURL"),
        callId: this.get("callId"),
        websocketToken: this.get("websocketToken")
      });

      this._websocket.promiseConnect().then(
        function() {
          this.dispatcher.dispatch(new sharedActions.ConnectionProgress({
            // This is the websocket call state, i.e. waiting for the
            // other end to connect to the server.
            state: "connecting"
          }));
        }.bind(this),
        function(error) {
          console.error("Websocket failed to connect", error);
          this.dispatcher.dispatch(new sharedActions.ConnectionFailure({
            reason: "websocket-setup"
          }));
        }.bind(this)
      );

      this._websocket.on("progress", this._handleWebSocketProgress, this);
    },

    /**
     * Used to handle any progressed received from the websocket. This will
     * dispatch new actions so that the data can be handled appropriately.
     */
    _handleWebSocketProgress: function(progressData) {
      var action;

      switch(progressData.state) {
        case "terminated":
          action = new sharedActions.ConnectionFailure({
            reason: progressData.reason
          });
          break;
        case "alerting":
          action = new sharedActions.ConnectionProgress({
            state: progressData.state
          });
          break;
        default:
          console.warn("Received unexpected state in _handleWebSocketProgress", progressData.state);
          return;
      }

      this.dispatcher.dispatch(action);
    }
  });

  return {
    CALL_STATES: CALL_STATES,
    ConversationStore: ConversationStore
  };
})();
