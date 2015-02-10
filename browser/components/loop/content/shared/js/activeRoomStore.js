/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.store = loop.store || {};

loop.store.ActiveRoomStore = (function() {
  "use strict";

  var sharedActions = loop.shared.actions;
  var FAILURE_DETAILS = loop.shared.utils.FAILURE_DETAILS;
  var SCREEN_SHARE_STATES = loop.shared.utils.SCREEN_SHARE_STATES;

  // Error numbers taken from
  // https://github.com/mozilla-services/loop-server/blob/master/loop/errno.json
  var REST_ERRNOS = loop.shared.utils.REST_ERRNOS;

  var ROOM_STATES = loop.store.ROOM_STATES;

  /**
   * Active room store.
   *
   * @param {loop.Dispatcher} dispatcher  The dispatcher for dispatching actions
   *                                      and registering to consume actions.
   * @param {Object} options Options object:
   * - {mozLoop}     mozLoop    The MozLoop API object.
   * - {OTSdkDriver} sdkDriver  The SDK driver instance.
   */
  var ActiveRoomStore = loop.store.createStore({
    /**
     * The time factor to adjust the expires time to ensure that we send a refresh
     * before the expiry. Currently set as 90%.
     */
    expiresTimeFactor: 0.9,

    // XXX Further actions are registered in setupWindowData and
    // fetchServerData when we know what window type this is. At some stage,
    // we might want to consider store mixins or some alternative which
    // means the stores would only be created when we want them.
    actions: [
      "setupWindowData",
      "fetchServerData"
    ],

    initialize: function(options) {
      if (!options.mozLoop) {
        throw new Error("Missing option mozLoop");
      }
      this._mozLoop = options.mozLoop;

      if (!options.sdkDriver) {
        throw new Error("Missing option sdkDriver");
      }
      this._sdkDriver = options.sdkDriver;
    },

    /**
     * Returns initial state data for this active room.
     */
    getInitialStoreState: function() {
      return {
        roomState: ROOM_STATES.INIT,
        audioMuted: false,
        videoMuted: false,
        failureReason: undefined,
        // Tracks if the room has been used during this
        // session. 'Used' means at least one call has been placed
        // with it. Entering and leaving the room without seeing
        // anyone is not considered as 'used'
        used: false,
        localVideoDimensions: {},
        remoteVideoDimensions: {},
        screenSharingState: SCREEN_SHARE_STATES.INACTIVE,
        receivingScreenShare: false
      };
    },

    /**
     * Handles a room failure.
     *
     * @param {sharedActions.RoomFailure} actionData
     */
    roomFailure: function(actionData) {
      function getReason(serverCode) {
        switch (serverCode) {
          case REST_ERRNOS.INVALID_TOKEN:
          case REST_ERRNOS.EXPIRED:
            return FAILURE_DETAILS.EXPIRED_OR_INVALID;
          default:
            return FAILURE_DETAILS.UNKNOWN;
        }
      }

      console.error("Error in state `" + this._storeState.roomState + "`:",
        actionData.error);

      this.setStoreState({
        error: actionData.error,
        failureReason: getReason(actionData.error.errno)
      });

      this._leaveRoom(actionData.error.errno === REST_ERRNOS.ROOM_FULL ?
          ROOM_STATES.FULL : ROOM_STATES.FAILED);
    },

    /**
     * Registers the actions with the dispatcher that this store is interested
     * in after the initial setup has been performed.
     */
    _registerPostSetupActions: function() {
      this.dispatcher.register(this, [
        "roomFailure",
        "setupRoomInfo",
        "updateRoomInfo",
        "gotMediaPermission",
        "joinRoom",
        "joinedRoom",
        "connectedToSdkServers",
        "connectionFailure",
        "setMute",
        "screenSharingState",
        "receivingScreenShare",
        "remotePeerDisconnected",
        "remotePeerConnected",
        "windowUnload",
        "leaveRoom",
        "feedbackComplete",
        "videoDimensionsChanged"
      ]);
    },

    /**
     * Execute setupWindowData event action from the dispatcher. This gets
     * the room data from the mozLoop api, and dispatches an UpdateRoomInfo event.
     * It also dispatches JoinRoom as this action is only applicable to the desktop
     * client, and needs to auto-join.
     *
     * @param {sharedActions.SetupWindowData} actionData
     */
    setupWindowData: function(actionData) {
      if (actionData.type !== "room") {
        // Nothing for us to do here, leave it to other stores.
        return;
      }

      this._registerPostSetupActions();

      this.setStoreState({
        roomState: ROOM_STATES.GATHER,
        windowId: actionData.windowId
      });

      // Get the window data from the mozLoop api.
      this._mozLoop.rooms.get(actionData.roomToken,
        function(error, roomData) {
          if (error) {
            this.dispatchAction(new sharedActions.RoomFailure({error: error}));
            return;
          }

          this.dispatchAction(new sharedActions.SetupRoomInfo({
            roomToken: actionData.roomToken,
            roomName: roomData.roomName,
            roomOwner: roomData.roomOwner,
            roomUrl: roomData.roomUrl
          }));

          // For the conversation window, we need to automatically
          // join the room.
          this.dispatchAction(new sharedActions.JoinRoom());
        }.bind(this));
    },

    /**
     * Execute fetchServerData event action from the dispatcher. Although
     * this is to fetch the server data - for rooms on the standalone client,
     * we don't actually need to get any data. Therefore we just save the
     * data that is given to us for when the user chooses to join the room.
     *
     * @param {sharedActions.FetchServerData} actionData
     */
    fetchServerData: function(actionData) {
      if (actionData.windowType !== "room") {
        // Nothing for us to do here, leave it to other stores.
        return;
      }

      this._registerPostSetupActions();

      this.setStoreState({
        roomToken: actionData.token,
        roomState: ROOM_STATES.READY
      });

      this._mozLoop.rooms.on("update:" + actionData.roomToken,
        this._handleRoomUpdate.bind(this));
      this._mozLoop.rooms.on("delete:" + actionData.roomToken,
        this._handleRoomDelete.bind(this));
    },

    /**
     * Handles the setupRoomInfo action. Sets up the initial room data and
     * sets the state to `READY`.
     *
     * @param {sharedActions.SetupRoomInfo} actionData
     */
    setupRoomInfo: function(actionData) {
      if (this._onUpdateListener) {
        console.error("Room info already set up!");
        return;
      }

      this.setStoreState({
        roomName: actionData.roomName,
        roomOwner: actionData.roomOwner,
        roomState: ROOM_STATES.READY,
        roomToken: actionData.roomToken,
        roomUrl: actionData.roomUrl
      });

      this._onUpdateListener = this._handleRoomUpdate.bind(this);
      this._onDeleteListener = this._handleRoomDelete.bind(this);

      this._mozLoop.rooms.on("update:" + actionData.roomToken, this._onUpdateListener);
      this._mozLoop.rooms.on("delete:" + actionData.roomToken, this._onDeleteListener);
    },

    /**
     * Handles the updateRoomInfo action. Updates the room data.
     *
     * @param {sharedActions.UpdateRoomInfo} actionData
     */
    updateRoomInfo: function(actionData) {
      this.setStoreState({
        roomName: actionData.roomName,
        roomOwner: actionData.roomOwner,
        roomUrl: actionData.roomUrl
      });
    },

    /**
     * Handles room updates notified by the mozLoop rooms API.
     *
     * @param {String} eventName The name of the event
     * @param {Object} roomData  The new roomData.
     */
    _handleRoomUpdate: function(eventName, roomData) {
      this.dispatchAction(new sharedActions.UpdateRoomInfo({
        roomName: roomData.roomName,
        roomOwner: roomData.roomOwner,
        roomUrl: roomData.roomUrl
      }));
    },

    /**
     * Handles the deletion of a room, notified by the mozLoop rooms API.
     *
     * @param {String} eventName The name of the event
     * @param {Object} roomData  The roomData of the deleted room
     */
    _handleRoomDelete: function(eventName, roomData) {
      this._sdkDriver.forceDisconnectAll(function() {
        window.close();
      });
    },

    /**
     * Handles the action to join to a room.
     */
    joinRoom: function() {
      // Reset the failure reason if necessary.
      if (this.getStoreState().failureReason) {
        this.setStoreState({failureReason: undefined});
      }

      this.setStoreState({roomState: ROOM_STATES.MEDIA_WAIT});
    },

    /**
     * Handles the action that signifies when media permission has been
     * granted and starts joining the room.
     */
    gotMediaPermission: function() {
      this.setStoreState({roomState: ROOM_STATES.JOINING});

      this._mozLoop.rooms.join(this._storeState.roomToken,
        function(error, responseData) {
          if (error) {
            this.dispatchAction(new sharedActions.RoomFailure({error: error}));
            return;
          }

          this.dispatchAction(new sharedActions.JoinedRoom({
            apiKey: responseData.apiKey,
            sessionToken: responseData.sessionToken,
            sessionId: responseData.sessionId,
            expires: responseData.expires
          }));
        }.bind(this));
    },

    /**
     * Handles the data received from joining a room. It stores the relevant
     * data, and sets up the refresh timeout for ensuring membership of the room
     * is refreshed regularly.
     *
     * @param {sharedActions.JoinedRoom} actionData
     */
    joinedRoom: function(actionData) {
      this.setStoreState({
        apiKey: actionData.apiKey,
        sessionToken: actionData.sessionToken,
        sessionId: actionData.sessionId,
        roomState: ROOM_STATES.JOINED
      });

      this._setRefreshTimeout(actionData.expires);
      this._sdkDriver.connectSession(actionData);

      this._mozLoop.addConversationContext(this._storeState.windowId,
                                           actionData.sessionId, "");

      // If we haven't got a room name yet, go and get one. We typically
      // need to do this in the case of the standalone window.
      // XXX When bug 1103331 lands this can be moved to earlier.
      if (!this._storeState.roomName) {
        this._mozLoop.rooms.get(this._storeState.roomToken,
          function(err, result) {
            if (err) {
              console.error("Failed to get room data:", err);
              return;
            }

            this.dispatcher.dispatch(new sharedActions.UpdateRoomInfo(result));
        }.bind(this));
      }
    },

    /**
     * Handles recording when the sdk has connected to the servers.
     */
    connectedToSdkServers: function() {
      this.setStoreState({
        roomState: ROOM_STATES.SESSION_CONNECTED
      });
    },

    /**
     * Handles disconnection of this local client from the sdk servers.
     *
     * @param {sharedActions.ConnectionFailure} actionData
     */
    connectionFailure: function(actionData) {
      // Treat all reasons as something failed. In theory, clientDisconnected
      // could be a success case, but there's no way we should be intentionally
      // sending that and still have the window open.
      this.setStoreState({
        failureReason: actionData.reason
      });

      this._leaveRoom(ROOM_STATES.FAILED);
    },

    /**
     * Records the mute state for the stream.
     *
     * @param {sharedActions.setMute} actionData The mute state for the stream type.
     */
    setMute: function(actionData) {
      var muteState = {};
      muteState[actionData.type + "Muted"] = !actionData.enabled;
      this.setStoreState(muteState);
    },

    /**
     * Used to note the current screensharing state.
     */
    screenSharingState: function(actionData) {
      this.setStoreState({screenSharingState: actionData.state});

      this._mozLoop.setScreenShareState(
        this.getStoreState().windowId,
        actionData.state === SCREEN_SHARE_STATES.ACTIVE);
    },

    /**
     * Used to note the current state of receiving screenshare data.
     */
    receivingScreenShare: function(actionData) {
      this.setStoreState({receivingScreenShare: actionData.receiving});
    },

    /**
     * Handles recording when a remote peer has connected to the servers.
     */
    remotePeerConnected: function() {
      this.setStoreState({
        roomState: ROOM_STATES.HAS_PARTICIPANTS,
        used: true
      });

      // We've connected with a third-party, therefore stop displaying the ToS etc.
      this._mozLoop.setLoopPref("seenToS", "seen");
    },

    /**
     * Handles a remote peer disconnecting from the session. As we currently only
     * support 2 participants, we declare the room as SESSION_CONNECTED as soon as
     * one participantleaves.
     */
    remotePeerDisconnected: function() {
      this.setStoreState({roomState: ROOM_STATES.SESSION_CONNECTED});
    },

    /**
     * Handles the window being unloaded. Ensures the room is left.
     */
    windowUnload: function() {
      this._leaveRoom(ROOM_STATES.CLOSING);

      // If we're closing the window, then ensure the screensharing state
      // is cleared. We don't do this on leave room, as we might still be
      // sharing.
      this._mozLoop.setScreenShareState(
        this.getStoreState().windowId,
        false);

      if (!this._onUpdateListener) {
        return;
      }

      // If we're closing the window, we can stop listening to updates.
      var roomToken = this.getStoreState().roomToken;
      this._mozLoop.rooms.off("update:" + roomToken, this._onUpdateListener);
      this._mozLoop.rooms.off("delete:" + roomToken, this._onDeleteListener);
      delete this._onUpdateListener;
      delete this._onDeleteListener;
    },

    /**
     * Handles a room being left.
     */
    leaveRoom: function() {
      this._leaveRoom();
    },

    /**
     * Handles setting of the refresh timeout callback.
     *
     * @param {Integer} expireTime The time until expiry (in seconds).
     */
    _setRefreshTimeout: function(expireTime) {
      this._timeout = setTimeout(this._refreshMembership.bind(this),
        expireTime * this.expiresTimeFactor * 1000);
    },

    /**
     * Refreshes the membership of the room with the server, and then
     * sets up the refresh for the next cycle.
     */
    _refreshMembership: function() {
      this._mozLoop.rooms.refreshMembership(this._storeState.roomToken,
        this._storeState.sessionToken,
        function(error, responseData) {
          if (error) {
            this.dispatchAction(new sharedActions.RoomFailure({error: error}));
            return;
          }

          this._setRefreshTimeout(responseData.expires);
        }.bind(this));
    },

    /**
     * Handles leaving a room. Clears any membership timeouts, then
     * signals to the server the leave of the room.
     *
     * @param {ROOM_STATES} nextState Optional; the next state to switch to.
     *                                Switches to READY if undefined.
     */
    _leaveRoom: function(nextState) {
      if (loop.standaloneMedia) {
        loop.standaloneMedia.multiplexGum.reset();
      }

      this._sdkDriver.disconnectSession();

      if (this._timeout) {
        clearTimeout(this._timeout);
        delete this._timeout;
      }

      if (this._storeState.roomState === ROOM_STATES.JOINING ||
          this._storeState.roomState === ROOM_STATES.JOINED ||
          this._storeState.roomState === ROOM_STATES.SESSION_CONNECTED ||
          this._storeState.roomState === ROOM_STATES.HAS_PARTICIPANTS) {
        this._mozLoop.rooms.leave(this._storeState.roomToken,
          this._storeState.sessionToken);
      }

      this.setStoreState({roomState: nextState || ROOM_STATES.ENDED});
    },

    /**
     * When feedback is complete, we reset the room to the initial state.
     */
    feedbackComplete: function() {
      // Note, that we want some values, such as the windowId, so we don't
      // do a full reset here.
      this.setStoreState(this.getInitialStoreState());
    },

    /**
     * Handles a change in dimensions of a video stream and updates the store data
     * with the new dimensions of a local or remote stream.
     *
     * @param {sharedActions.VideoDimensionsChanged} actionData
     */
    videoDimensionsChanged: function(actionData) {
      // NOTE: in the future, when multiple remote video streams are supported,
      //       we'll need to make this support multiple remotes as well. Good
      //       starting point for video tiling.
      var storeProp = (actionData.isLocal ? "local" : "remote") + "VideoDimensions";
      var nextState = {};
      nextState[storeProp] = this.getStoreState()[storeProp];
      nextState[storeProp][actionData.videoType] = actionData.dimensions;
      this.setStoreState(nextState);
    }
  });

  return ActiveRoomStore;
})();
