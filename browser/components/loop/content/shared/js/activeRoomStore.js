/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.store = loop.store || {};

loop.store.ROOM_STATES = {
    // The initial state of the room
    INIT: "room-init",
    // The store is gathering the room data
    GATHER: "room-gather",
    // The store has got the room data
    READY: "room-ready",
    // Obtaining media from the user
    MEDIA_WAIT: "room-media-wait",
    // Joining the room is taking place
    JOINING: "room-joining",
    // The room is known to be joined on the loop-server
    JOINED: "room-joined",
    // The room is connected to the sdk server.
    SESSION_CONNECTED: "room-session-connected",
    // There are participants in the room.
    HAS_PARTICIPANTS: "room-has-participants",
    // There was an issue with the room
    FAILED: "room-failed",
    // The room is full
    FULL: "room-full",
    // The room conversation has ended, displays the feedback view.
    ENDED: "room-ended",
    // The window is closing
    CLOSING: "room-closing"
};

loop.store.ActiveRoomStore = (function() {
  "use strict";

  var sharedActions = loop.shared.actions;
  var crypto = loop.crypto;
  var FAILURE_DETAILS = loop.shared.utils.FAILURE_DETAILS;
  var SCREEN_SHARE_STATES = loop.shared.utils.SCREEN_SHARE_STATES;

  // Error numbers taken from
  // https://github.com/mozilla-services/loop-server/blob/master/loop/errno.json
  var REST_ERRNOS = loop.shared.utils.REST_ERRNOS;

  var ROOM_STATES = loop.store.ROOM_STATES;

  var ROOM_INFO_FAILURES = loop.shared.utils.ROOM_INFO_FAILURES;

  var OPTIONAL_ROOMINFO_FIELDS = {
    urls: "roomContextUrls",
    description: "roomDescription",
    participants: "participants",
    roomInfoFailure: "roomInfoFailure",
    roomName: "roomName",
    roomState: "roomState"
  };

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

      this._isDesktop = options.isDesktop || false;
    },

    /**
     * This is a list of states that need resetting when the room is left,
     * due to user choice, failure or other reason. It is a subset of
     * getInitialStoreState as some items (e.g. roomState, failureReason,
     * context information) can persist across room exit & re-entry.
     *
     * @type {Array}
     */
    _statesToResetOnLeave: [
      "audioMuted",
      "localSrcVideoObject",
      "localVideoDimensions",
      "mediaConnected",
      "receivingScreenShare",
      "remoteSrcVideoObject",
      "remoteVideoDimensions",
      "remoteVideoEnabled",
      "screenSharingState",
      "screenShareVideoObject",
      "videoMuted"
    ],

    /**
     * Returns initial state data for this active room.
     *
     * When adding states, consider if _statesToResetOnLeave needs updating
     * as well.
     */
    getInitialStoreState: function() {
      return {
        roomState: ROOM_STATES.INIT,
        audioMuted: false,
        videoMuted: false,
        remoteVideoEnabled: false,
        failureReason: undefined,
        // Tracks if the room has been used during this
        // session. 'Used' means at least one call has been placed
        // with it. Entering and leaving the room without seeing
        // anyone is not considered as 'used'
        used: false,
        localVideoDimensions: {},
        remoteVideoDimensions: {},
        screenSharingState: SCREEN_SHARE_STATES.INACTIVE,
        receivingScreenShare: false,
        // Any urls (aka context) associated with the room.
        roomContextUrls: null,
        // The roomCryptoKey to decode the context data if necessary.
        roomCryptoKey: null,
        // The description for a room as stored in the context data.
        roomDescription: null,
        // Room information failed to be obtained for a reason. See ROOM_INFO_FAILURES.
        roomInfoFailure: null,
        // The name of the room.
        roomName: null,
        // Social API state.
        socialShareProviders: null,
        // True if media has been connected both-ways.
        mediaConnected: false
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

      var exitState = this._storeState.roomState !== ROOM_STATES.FAILED ?
        this._storeState.roomState : this._storeState.failureExitState;

      this.setStoreState({
        error: actionData.error,
        failureReason: getReason(actionData.error.errno),
        failureExitState: exitState
      });

      this._leaveRoom(actionData.error.errno === REST_ERRNOS.ROOM_FULL ?
          ROOM_STATES.FULL : ROOM_STATES.FAILED, actionData.failedJoinRequest);
    },

    /**
     * Attempts to retry getting the room data if there has been a room failure.
     */
    retryAfterRoomFailure: function() {
      if (this._storeState.failureReason === FAILURE_DETAILS.EXPIRED_OR_INVALID) {
        console.error("Invalid retry attempt for expired or invalid url");
        return;
      }

      switch (this._storeState.failureExitState) {
        case ROOM_STATES.GATHER:
          this.dispatchAction(new sharedActions.FetchServerData({
            cryptoKey: this._storeState.roomCryptoKey,
            token: this._storeState.roomToken,
            windowType: "room"
          }));
          return;
        case ROOM_STATES.INIT:
        case ROOM_STATES.ENDED:
        case ROOM_STATES.CLOSING:
          console.error("Unexpected retry for exit state", this._storeState.failureExitState);
          return;
        default:
          // For all other states, we simply join the room. We avoid dispatching
          // another action here so that metrics doesn't get two notifications
          // in a row (one for retry, one for the join).
          this.joinRoom();
          return;
      }
    },

    /**
     * Registers the actions with the dispatcher that this store is interested
     * in after the initial setup has been performed.
     */
    _registerPostSetupActions: function() {
      // Protect against calling this twice, as we don't want to register
      // before we know what type we are, but in some cases we need to re-do
      // an action (e.g. FetchServerData).
      if (this._registeredActions) {
        return;
      }

      this._registeredActions = true;

      this.dispatcher.register(this, [
        "roomFailure",
        "retryAfterRoomFailure",
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
        "localVideoEnabled",
        "remoteVideoEnabled",
        "remoteVideoDisabled",
        "videoDimensionsChanged",
        "startScreenShare",
        "endScreenShare",
        "updateSocialShareInfo",
        "connectionStatus",
        "mediaConnected"
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
            this.dispatchAction(new sharedActions.RoomFailure({
              error: error,
              failedJoinRequest: false
            }));
            return;
          }

          this.dispatchAction(new sharedActions.SetupRoomInfo({
            participants: roomData.participants,
            roomToken: actionData.roomToken,
            roomContextUrls: roomData.decryptedContext.urls,
            roomDescription: roomData.decryptedContext.description,
            roomName: roomData.decryptedContext.roomName,
            roomUrl: roomData.roomUrl,
            socialShareProviders: this._mozLoop.getSocialShareProviders()
          }));

          // For the conversation window, we need to automatically
          // join the room.
          this.dispatchAction(new sharedActions.JoinRoom());
        }.bind(this));
    },

    /**
     * Execute fetchServerData event action from the dispatcher. For rooms
     * we need to get the room context information from the server. We don't
     * need other data until the user decides to join the room.
     * This action is only used for the standalone UI.
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
        roomState: ROOM_STATES.GATHER,
        roomCryptoKey: actionData.cryptoKey
      });

      this._mozLoop.rooms.on("update:" + actionData.roomToken,
        this._handleRoomUpdate.bind(this));
      this._mozLoop.rooms.on("delete:" + actionData.roomToken,
        this._handleRoomDelete.bind(this));

      this._getRoomDataForStandalone();
    },

    _getRoomDataForStandalone: function() {
      this._mozLoop.rooms.get(this._storeState.roomToken, function(err, result) {
        if (err) {
          this.dispatchAction(new sharedActions.RoomFailure({
            error: err,
            failedJoinRequest: false
          }));
          return;
        }

        var roomInfoData = new sharedActions.UpdateRoomInfo({
          roomUrl: result.roomUrl
        });

        // If we've got this far, then we want to go to the ready state
        // regardless of success of failure. This is because failures of
        // crypto don't stop the user using the room, they just stop
        // us putting up the information.
        roomInfoData.roomState = ROOM_STATES.READY;

        if (!result.context && !result.roomName) {
          roomInfoData.roomInfoFailure = ROOM_INFO_FAILURES.NO_DATA;
          this.dispatcher.dispatch(roomInfoData);
          return;
        }

        // This handles 'legacy', non-encrypted room names.
        if (result.roomName && !result.context) {
          roomInfoData.roomName = result.roomName;
          this.dispatcher.dispatch(roomInfoData);
          return;
        }

        if (!crypto.isSupported()) {
          roomInfoData.roomInfoFailure = ROOM_INFO_FAILURES.WEB_CRYPTO_UNSUPPORTED;
          this.dispatcher.dispatch(roomInfoData);
          return;
        }

        var roomCryptoKey = this.getStoreState("roomCryptoKey");

        if (!roomCryptoKey) {
          roomInfoData.roomInfoFailure = ROOM_INFO_FAILURES.NO_CRYPTO_KEY;
          this.dispatcher.dispatch(roomInfoData);
          return;
        }

        var dispatcher = this.dispatcher;

        crypto.decryptBytes(roomCryptoKey, result.context.value)
              .then(function(decryptedResult) {
          var realResult = JSON.parse(decryptedResult);

          roomInfoData.description = realResult.description;
          roomInfoData.urls = realResult.urls;
          roomInfoData.roomName = realResult.roomName;

          dispatcher.dispatch(roomInfoData);
        }, function(error) {
          roomInfoData.roomInfoFailure = ROOM_INFO_FAILURES.DECRYPT_FAILED;
          dispatcher.dispatch(roomInfoData);
        });
      }.bind(this));
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
        participants: actionData.participants,
        roomContextUrls: actionData.roomContextUrls,
        roomDescription: actionData.roomDescription,
        roomName: actionData.roomName,
        roomState: ROOM_STATES.READY,
        roomToken: actionData.roomToken,
        roomUrl: actionData.roomUrl,
        socialShareProviders: actionData.socialShareProviders
      });

      this._onUpdateListener = this._handleRoomUpdate.bind(this);
      this._onDeleteListener = this._handleRoomDelete.bind(this);
      this._onSocialShareUpdate = this._handleSocialShareUpdate.bind(this);

      this._mozLoop.rooms.on("update:" + actionData.roomToken, this._onUpdateListener);
      this._mozLoop.rooms.on("delete:" + actionData.roomToken, this._onDeleteListener);
      window.addEventListener("LoopShareWidgetChanged", this._onSocialShareUpdate);
      window.addEventListener("LoopSocialProvidersChanged", this._onSocialShareUpdate);
    },

    /**
     * Handles the updateRoomInfo action. Updates the room data.
     *
     * @param {sharedActions.UpdateRoomInfo} actionData
     */
    updateRoomInfo: function(actionData) {
      var newState = {
        roomUrl: actionData.roomUrl
      };
      // Iterate over the optional fields that _may_ be present on the actionData
      // object.
      Object.keys(OPTIONAL_ROOMINFO_FIELDS).forEach(function(field) {
        if (actionData[field] !== undefined) {
          newState[OPTIONAL_ROOMINFO_FIELDS[field]] = actionData[field];
        }
      });
      this.setStoreState(newState);
    },

    /**
     * Handles the updateSocialShareInfo action. Updates the room data with new
     * Social API info.
     *
     * @param  {sharedActions.UpdateSocialShareInfo} actionData
     */
    updateSocialShareInfo: function(actionData) {
      this.setStoreState({
        socialShareProviders: actionData.socialShareProviders
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
        urls: roomData.decryptedContext.urls,
        description: roomData.decryptedContext.description,
        participants: roomData.participants,
        roomName: roomData.decryptedContext.roomName,
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
     * Handles an update of the position of the Share widget and changes to list
     * of Social API providers, notified by the mozLoop API.
     */
    _handleSocialShareUpdate: function() {
      this.dispatchAction(new sharedActions.UpdateSocialShareInfo({
        socialShareProviders: this._mozLoop.getSocialShareProviders()
      }));
    },

    /**
     * Handles the action to join to a room.
     */
    joinRoom: function() {
      // Reset the failure reason if necessary.
      if (this.getStoreState().failureReason) {
        this.setStoreState({failureReason: undefined});
      }

      // XXX Ideally we'd do this check before joining a room, but we're waiting
      // for the UX for that. See bug 1166824. In the meantime this gives us
      // additional information for analysis.
      loop.shared.utils.hasAudioOrVideoDevices(function(hasDevices) {
        if (hasDevices) {
          // MEDIA_WAIT causes the views to dispatch sharedActions.SetupStreamElements,
          // which in turn starts the sdk obtaining the device permission.
          this.setStoreState({roomState: ROOM_STATES.MEDIA_WAIT});
        } else {
          this.dispatchAction(new sharedActions.ConnectionFailure({
            reason: FAILURE_DETAILS.NO_MEDIA
          }));
        }
      }.bind(this));
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
            this.dispatchAction(new sharedActions.RoomFailure({
              error: error,
              // This is an explicit flag to avoid the leave happening if join
              // fails. We can't track it on ROOM_STATES.JOINING as the user
              // might choose to leave the room whilst the XHR is in progress
              // which would then mean we'd run the race condition of not
              // notifying the server of a leave.
              failedJoinRequest: true
            }));
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

      // Only send media telemetry on one side of the call: the desktop side.
      actionData.sendTwoWayMediaTelemetry = this._isDesktop;

      this._sdkDriver.connectSession(actionData);

      this._mozLoop.addConversationContext(this._storeState.windowId,
                                           actionData.sessionId, "");
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
      var exitState = this._storeState.roomState === ROOM_STATES.FAILED ?
        this._storeState.failureExitState : this._storeState.roomState;

      // Treat all reasons as something failed. In theory, clientDisconnected
      // could be a success case, but there's no way we should be intentionally
      // sending that and still have the window open.
      this.setStoreState({
        failureReason: actionData.reason,
        failureExitState: exitState
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
     * Records the local video object for the room.
     *
     * @param {sharedActions.LocalVideoEnabled} actionData
     */
    localVideoEnabled: function(actionData) {
      this.setStoreState({localSrcVideoObject: actionData.srcVideoObject});
    },

    /**
     * Records the remote video object for the room.
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
     * Records when remote video is disabled (e.g. due to mute).
     */
    remoteVideoDisabled: function() {
      this.setStoreState({remoteVideoEnabled: false});
    },

    /**
     * Records when the remote media has been connected.
     */
    mediaConnected: function() {
      this.setStoreState({mediaConnected: true});
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
     *
     * XXX this should be split into multiple actions to make the code clearer.
     */
    receivingScreenShare: function(actionData) {
      if (!actionData.receiving &&
          this.getStoreState().remoteVideoDimensions.screen) {
        // Remove the remote video dimensions for type screen as we're not
        // getting the share anymore.
        var newDimensions = _.extend(this.getStoreState().remoteVideoDimensions);
        delete newDimensions.screen;
        this.setStoreState({
          receivingScreenShare: actionData.receiving,
          remoteVideoDimensions: newDimensions,
          screenShareVideoObject: null
        });
      } else {
        this.setStoreState({
          receivingScreenShare: actionData.receiving,
          screenShareVideoObject: actionData.srcVideoObject ?
                                  actionData.srcVideoObject : null
        });
      }
    },

    /**
     * Handles switching browser (aka tab) sharing to a new window. Should
     * only be used for browser sharing.
     *
     * @param {Number} windowId  The new windowId to start sharing.
     */
    _handleSwitchBrowserShare: function(err, windowId) {
      if (err) {
        console.error("Error getting the windowId: " + err);
        this.dispatchAction(new sharedActions.ScreenSharingState({
          state: SCREEN_SHARE_STATES.INACTIVE
        }));
        return;
      }

      var screenSharingState = this.getStoreState().screenSharingState;

      if (screenSharingState === SCREEN_SHARE_STATES.INACTIVE) {
        // Screen sharing is still pending, so assume that we need to kick it off.
        var options = {
          videoSource: "browser",
          constraints: {
            browserWindow: windowId,
            scrollWithPage: true
          }
        };
        this._sdkDriver.startScreenShare(options);
      } else if (screenSharingState === SCREEN_SHARE_STATES.ACTIVE) {
        // Just update the current share.
        this._sdkDriver.switchAcquiredWindow(windowId);
      } else {
        console.error("Unexpectedly received windowId for browser sharing when pending");
      }
    },

    /**
     * Initiates a screen sharing publisher.
     *
     * @param {sharedActions.StartScreenShare} actionData
     */
    startScreenShare: function(actionData) {
      this.dispatchAction(new sharedActions.ScreenSharingState({
        state: SCREEN_SHARE_STATES.PENDING
      }));

      var options = {
        videoSource: actionData.type
      };
      if (options.videoSource === "browser") {
        this._browserSharingListener = this._handleSwitchBrowserShare.bind(this);

        // Set up a listener for watching screen shares. This will get notified
        // with the first windowId when it is added, so we start off the sharing
        // from within the listener.
        this._mozLoop.addBrowserSharingListener(this._browserSharingListener);
      } else {
        this._sdkDriver.startScreenShare(options);
      }
    },

    /**
     * Ends an active screenshare session.
     */
    endScreenShare: function() {
      if (this._browserSharingListener) {
        // Remove the browser sharing listener as we don't need it now.
        this._mozLoop.removeBrowserSharingListener(this._browserSharingListener);
        this._browserSharingListener = null;
      }

      if (this._sdkDriver.endScreenShare()) {
        this.dispatchAction(new sharedActions.ScreenSharingState({
          state: SCREEN_SHARE_STATES.INACTIVE
        }));
      }
    },

    /**
     * Handles recording when a remote peer has connected to the servers.
     */
    remotePeerConnected: function() {
      this.setStoreState({
        roomState: ROOM_STATES.HAS_PARTICIPANTS,
        used: true
      });
    },

    /**
     * Handles a remote peer disconnecting from the session. As we currently only
     * support 2 participants, we declare the room as SESSION_CONNECTED as soon as
     * one participantleaves.
     */
    remotePeerDisconnected: function() {
      // Update the participants to just the owner.
      var participants = this.getStoreState("participants");
      if (participants) {
        participants = participants.filter(function(participant) {
          return participant.owner;
        });
      }

      this.setStoreState({
        participants: participants,
        roomState: ROOM_STATES.SESSION_CONNECTED,
        remoteSrcVideoObject: null
      });
    },

    /**
     * Handles an SDK status update, forwarding it to the server.
     *
     * @param {sharedActions.ConnectionStatus} actionData
     */
    connectionStatus: function(actionData) {
      this._mozLoop.rooms.sendConnectionStatus(this.getStoreState("roomToken"),
        this.getStoreState("sessionToken"),
        actionData);
    },

    /**
     * Handles the window being unloaded. Ensures the room is left.
     */
    windowUnload: function() {
      this._leaveRoom(ROOM_STATES.CLOSING);

      if (!this._onUpdateListener) {
        return;
      }

      // If we're closing the window, we can stop listening to updates.
      var roomToken = this.getStoreState().roomToken;
      this._mozLoop.rooms.off("update:" + roomToken, this._onUpdateListener);
      this._mozLoop.rooms.off("delete:" + roomToken, this._onDeleteListener);
      window.removeEventListener("LoopShareWidgetChanged", this._onShareWidgetUpdate);
      window.removeEventListener("LoopSocialProvidersChanged", this._onSocialProvidersUpdate);
      delete this._onUpdateListener;
      delete this._onDeleteListener;
      delete this._onShareWidgetUpdate;
      delete this._onSocialProvidersUpdate;
    },

    /**
     * Handles a room being left.
     */
    leaveRoom: function() {
      this._leaveRoom(ROOM_STATES.ENDED);
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
            this.dispatchAction(new sharedActions.RoomFailure({
              error: error,
              failedJoinRequest: false
            }));
            return;
          }

          this._setRefreshTimeout(responseData.expires);
        }.bind(this));
    },

    /**
     * Handles leaving a room. Clears any membership timeouts, then
     * signals to the server the leave of the room.
     *
     * @param {ROOM_STATES} nextState         The next state to switch to.
     * @param {Boolean}     failedJoinRequest Optional. Set to true if the join
     *                                        request to loop-server failed. It
     *                                        will skip the leave message.
     */
    _leaveRoom: function(nextState, failedJoinRequest) {
      if (loop.standaloneMedia) {
        loop.standaloneMedia.multiplexGum.reset();
      }

      this._mozLoop.setScreenShareState(
        this.getStoreState().windowId,
        false);

      if (this._browserSharingListener) {
        // Remove the browser sharing listener as we don't need it now.
        this._mozLoop.removeBrowserSharingListener(this._browserSharingListener);
        this._browserSharingListener = null;
      }

      // We probably don't need to end screen share separately, but lets be safe.
      this._sdkDriver.disconnectSession();

      // Reset various states.
      var originalStoreState = this.getInitialStoreState();
      var newStoreState = {};

      this._statesToResetOnLeave.forEach(function(state) {
        newStoreState[state] = originalStoreState[state];
      });
      this.setStoreState(newStoreState);

      if (this._timeout) {
        clearTimeout(this._timeout);
        delete this._timeout;
      }

      if (!failedJoinRequest &&
          (this._storeState.roomState === ROOM_STATES.JOINING ||
           this._storeState.roomState === ROOM_STATES.JOINED ||
           this._storeState.roomState === ROOM_STATES.SESSION_CONNECTED ||
           this._storeState.roomState === ROOM_STATES.HAS_PARTICIPANTS)) {
        this._mozLoop.rooms.leave(this._storeState.roomToken,
          this._storeState.sessionToken);
      }

      this.setStoreState({roomState: nextState});
    },

    /**
     * When feedback is complete, we go back to the ready state, rather than
     * init or gather, as we don't need to get the data from the server again.
     */
    feedbackComplete: function() {
      this.setStoreState({
        roomState: ROOM_STATES.READY,
        // Reset the used state here as the user has now given feedback and the
        // next time they enter the room, the other person might not be there.
        used: false
      });
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
