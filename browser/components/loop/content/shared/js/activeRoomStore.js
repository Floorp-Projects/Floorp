/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.store = loop.store || {};

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
        receivingScreenShare: false,
        // Any urls (aka context) associated with the room.
        roomContextUrls: null,
        // The roomCryptoKey to decode the context data if necessary.
        roomCryptoKey: null,
        // Room information failed to be obtained for a reason. See ROOM_INFO_FAILURES.
        roomInfoFailure: null,
        // The name of the room.
        roomName: null,
        // Social API state.
        socialShareButtonAvailable: false,
        socialShareProviders: null
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
          ROOM_STATES.FULL : ROOM_STATES.FAILED, actionData.failedJoinRequest);
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
        "videoDimensionsChanged",
        "startScreenShare",
        "endScreenShare",
        "updateSocialShareInfo"
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
            roomToken: actionData.roomToken,
            roomName: roomData.decryptedContext.roomName,
            roomOwner: roomData.roomOwner,
            roomUrl: roomData.roomUrl,
            socialShareButtonAvailable: this._mozLoop.isSocialShareButtonAvailable(),
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
        roomCryptoKey: actionData.cryptoKey,
        roomState: ROOM_STATES.READY
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
          // XXX Bug 1110937 will want to handle the error results here
          // e.g. room expired/invalid.
          console.error("Failed to get room data:", err);
          return;
        }

        var roomInfoData = new sharedActions.UpdateRoomInfo({
          roomOwner: result.roomOwner,
          roomUrl: result.roomUrl
        });

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

          roomInfoData.urls = realResult.urls;
          roomInfoData.roomName = realResult.roomName;

          dispatcher.dispatch(roomInfoData);
        }, function(err) {
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
        roomName: actionData.roomName,
        roomOwner: actionData.roomOwner,
        roomState: ROOM_STATES.READY,
        roomToken: actionData.roomToken,
        roomUrl: actionData.roomUrl,
        socialShareButtonAvailable: actionData.socialShareButtonAvailable,
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
      this.setStoreState({
        roomContextUrls: actionData.urls,
        roomInfoFailure: actionData.roomInfoFailure,
        roomName: actionData.roomName,
        roomOwner: actionData.roomOwner,
        roomUrl: actionData.roomUrl
      });
    },

    /**
     * Handles the updateSocialShareInfo action. Updates the room data with new
     * Social API info.
     *
     * @param  {sharedActions.UpdateSocialShareInfo} actionData
     */
    updateSocialShareInfo: function(actionData) {
      this.setStoreState({
        socialShareButtonAvailable: actionData.socialShareButtonAvailable,
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
        roomName: roomData.decryptedContext.roomName,
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
     * Handles an update of the position of the Share widget and changes to list
     * of Social API providers, notified by the mozLoop API.
     */
    _handleSocialShareUpdate: function() {
      this.dispatchAction(new sharedActions.UpdateSocialShareInfo({
        socialShareButtonAvailable: this._mozLoop.isSocialShareButtonAvailable(),
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
      actionData["sendTwoWayMediaTelemetry"] = this._isDesktop;

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
        this._sdkDriver.retryPublishWithoutVideo();
        return;
      }

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
          },
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
