/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.roomViews = function (mozL10n) {
  "use strict";

  var ROOM_STATES = loop.store.ROOM_STATES;
  var FAILURE_DETAILS = loop.shared.utils.FAILURE_DETAILS;
  var sharedActions = loop.shared.actions;
  var sharedMixins = loop.shared.mixins;
  var sharedDesktopViews = loop.shared.desktopViews;
  var sharedViews = loop.shared.views;

  /**
   * ActiveRoomStore mixin.
   * @type {Object}
   */
  var ActiveRoomStoreMixin = {
    mixins: [Backbone.Events],

    propTypes: {
      roomStore: React.PropTypes.instanceOf(loop.store.RoomStore).isRequired
    },

    componentWillMount: function () {
      this.listenTo(this.props.roomStore, "change:activeRoom", this._onActiveRoomStateChanged);
      this.listenTo(this.props.roomStore, "change:error", this._onRoomError);
      this.listenTo(this.props.roomStore, "change:savingContext", this._onRoomSavingContext);
    },

    componentWillUnmount: function () {
      this.stopListening(this.props.roomStore);
    },

    _onActiveRoomStateChanged: function () {
      // Only update the state if we're mounted, to avoid the problem where
      // stopListening doesn't nuke the active listeners during a event
      // processing.
      if (this.isMounted()) {
        this.setState(this.props.roomStore.getStoreState("activeRoom"));
      }
    },

    _onRoomError: function () {
      // Only update the state if we're mounted, to avoid the problem where
      // stopListening doesn't nuke the active listeners during a event
      // processing.
      if (this.isMounted()) {
        this.setState({ error: this.props.roomStore.getStoreState("error") });
      }
    },

    _onRoomSavingContext: function () {
      // Only update the state if we're mounted, to avoid the problem where
      // stopListening doesn't nuke the active listeners during a event
      // processing.
      if (this.isMounted()) {
        this.setState({ savingContext: this.props.roomStore.getStoreState("savingContext") });
      }
    },

    getInitialState: function () {
      var storeState = this.props.roomStore.getStoreState("activeRoom");
      return _.extend({
        // Used by the UI showcase.
        roomState: this.props.roomState || storeState.roomState,
        savingContext: false
      }, storeState);
    }
  };

  /**
   * Used to display errors in direct calls and rooms to the user.
   */
  var FailureInfoView = React.createClass({
    displayName: "FailureInfoView",

    propTypes: {
      failureReason: React.PropTypes.string.isRequired
    },

    /**
     * Returns the translated message appropraite to the failure reason.
     *
     * @return {String} The translated message for the failure reason.
     */
    _getMessage: function () {
      switch (this.props.failureReason) {
        case FAILURE_DETAILS.NO_MEDIA:
        case FAILURE_DETAILS.UNABLE_TO_PUBLISH_MEDIA:
          return mozL10n.get("no_media_failure_message");
        case FAILURE_DETAILS.TOS_FAILURE:
          return mozL10n.get("tos_failure_message", { clientShortname: mozL10n.get("clientShortname2") });
        case FAILURE_DETAILS.ICE_FAILED:
          return mozL10n.get("ice_failure_message");
        default:
          return mozL10n.get("generic_failure_message");
      }
    },

    render: function () {
      return React.createElement(
        "div",
        { className: "failure-info" },
        React.createElement("div", { className: "failure-info-logo" }),
        React.createElement(
          "h2",
          { className: "failure-info-message" },
          this._getMessage()
        )
      );
    }
  });

  /**
   * Something went wrong view. Displayed when there's a big problem.
   */
  var RoomFailureView = React.createClass({
    displayName: "RoomFailureView",

    mixins: [sharedMixins.AudioMixin],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      failureReason: React.PropTypes.string
    },

    componentDidMount: function () {
      this.play("failure");
    },

    handleRejoinCall: function () {
      this.props.dispatcher.dispatch(new sharedActions.JoinRoom());
    },

    render: function () {
      var btnTitle;
      if (this.props.failureReason === FAILURE_DETAILS.ICE_FAILED) {
        btnTitle = mozL10n.get("retry_call_button");
      } else {
        btnTitle = mozL10n.get("rejoin_button");
      }

      return React.createElement(
        "div",
        { className: "room-failure" },
        React.createElement(FailureInfoView, { failureReason: this.props.failureReason }),
        React.createElement(
          "div",
          { className: "btn-group call-action-group" },
          React.createElement(
            "button",
            { className: "btn btn-info btn-rejoin",
              onClick: this.handleRejoinCall },
            btnTitle
          )
        )
      );
    }
  });

  /**
   * Desktop room conversation view.
   */
  var DesktopRoomConversationView = React.createClass({
    displayName: "DesktopRoomConversationView",

    mixins: [ActiveRoomStoreMixin, sharedMixins.DocumentTitleMixin, sharedMixins.MediaSetupMixin, sharedMixins.RoomsAudioMixin, sharedMixins.WindowCloseMixin],

    propTypes: {
      chatWindowDetached: React.PropTypes.bool.isRequired,
      cursorStore: React.PropTypes.instanceOf(loop.store.RemoteCursorStore).isRequired,
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      facebookEnabled: React.PropTypes.bool.isRequired,
      // The poster URLs are for UI-showcase testing and development.
      localPosterUrl: React.PropTypes.string,
      onCallTerminated: React.PropTypes.func.isRequired,
      remotePosterUrl: React.PropTypes.string,
      roomStore: React.PropTypes.instanceOf(loop.store.RoomStore).isRequired
    },

    componentWillUpdate: function (nextProps, nextState) {
      // The SDK needs to know about the configuration and the elements to use
      // for display. So the best way seems to pass the information here - ideally
      // the sdk wouldn't need to know this, but we can't change that.
      if (this.state.roomState !== ROOM_STATES.MEDIA_WAIT && nextState.roomState === ROOM_STATES.MEDIA_WAIT) {
        this.props.dispatcher.dispatch(new sharedActions.SetupStreamElements({
          publisherConfig: this.getDefaultPublisherConfig({
            publishVideo: !this.state.videoMuted
          })
        }));
      }

      // Now that we're ready to share, automatically start sharing a tab only
      // if we're not already connected to the room via the sdk, e.g. not in the
      // case a remote participant just left.
      if (nextState.roomState === ROOM_STATES.SESSION_CONNECTED && !(this.state.roomState === ROOM_STATES.SESSION_CONNECTED || this.state.roomState === ROOM_STATES.HAS_PARTICIPANTS)) {
        this.props.dispatcher.dispatch(new sharedActions.StartBrowserShare());
      }
    },

    /**
     * User clicked on the "Leave" button.
     */
    leaveRoom: function () {
      if (this.state.used) {
        // The room has been used, so we might want to display the feedback view.
        // Therefore we leave the room to ensure that we have stopped sharing etc,
        // then trigger the call terminated handler that'll do the right thing
        // for the feedback view.
        this.props.dispatcher.dispatch(new sharedActions.LeaveRoom());
        this.props.onCallTerminated();
      } else {
        // If the room hasn't been used, we simply close the window.
        this.closeWindow();
      }
    },

    /**
     * Determine if the invitation controls should be shown.
     *
     * @return {Boolean} True if there's no guests.
     */
    _shouldRenderInvitationOverlay: function () {
      var hasGuests = typeof this.state.participants === "object" && this.state.participants.filter(function (participant) {
        return !participant.owner;
      }).length > 0;

      // Don't show if the room has participants whether from the room state or
      // there being non-owner guests in the participants array.
      return this.state.roomState !== ROOM_STATES.HAS_PARTICIPANTS && !hasGuests;
    },

    /**
     * Works out if remote video should be rended or not, depending on the
     * room state and other flags.
     *
     * @return {Boolean} True if remote video should be rended.
     *
     * XXX Refactor shouldRenderRemoteVideo & shouldRenderLoading into one fn
     *     that returns an enum
     */
    shouldRenderRemoteVideo: function () {
      switch (this.state.roomState) {
        case ROOM_STATES.HAS_PARTICIPANTS:
          if (this.state.remoteVideoEnabled) {
            return true;
          }

          if (this.state.mediaConnected) {
            // since the remoteVideo hasn't yet been enabled, if the
            // media is connected, then we should be displaying an avatar.
            return false;
          }

          return true;

        case ROOM_STATES.READY:
        case ROOM_STATES.GATHER:
        case ROOM_STATES.INIT:
        case ROOM_STATES.JOINING:
        case ROOM_STATES.SESSION_CONNECTED:
        case ROOM_STATES.JOINED:
        case ROOM_STATES.MEDIA_WAIT:
          // this case is so that we don't show an avatar while waiting for
          // the other party to connect
          return true;

        case ROOM_STATES.CLOSING:
          return true;

        default:
          console.warn("DesktopRoomConversationView.shouldRenderRemoteVideo:" + " unexpected roomState: ", this.state.roomState);
          return true;
      }
    },

    /**
     * Should we render a visual cue to the user (e.g. a spinner) that a local
     * stream is on its way from the camera?
     *
     * @returns {boolean}
     * @private
     */
    _isLocalLoading: function () {
      return this.state.roomState === ROOM_STATES.MEDIA_WAIT && !this.state.localSrcMediaElement;
    },

    /**
     * Should we render a visual cue to the user (e.g. a spinner) that a remote
     * stream is on its way from the other user?
     *
     * @returns {boolean}
     * @private
     */
    _isRemoteLoading: function () {
      return !!(this.state.roomState === ROOM_STATES.HAS_PARTICIPANTS && !this.state.remoteSrcMediaElement && !this.state.mediaConnected);
    },

    handleContextMenu: function (e) {
      e.preventDefault();
    },

    render: function () {
      if (this.state.roomName || this.state.roomContextUrls) {
        var roomTitle = this.state.roomName || this.state.roomContextUrls[0].description || this.state.roomContextUrls[0].location;
        if (!roomTitle) {
          roomTitle = mozL10n.get("room_name_untitled_page");
        }
        this.setTitle(roomTitle);
      }

      var shouldRenderInvitationOverlay = this._shouldRenderInvitationOverlay();
      var roomData = this.props.roomStore.getStoreState("activeRoom");

      switch (this.state.roomState) {
        case ROOM_STATES.FAILED:
        case ROOM_STATES.FULL:
          {
            // Note: While rooms are set to hold a maximum of 2 participants, the
            //       FULL case should never happen on desktop.
            return React.createElement(RoomFailureView, {
              dispatcher: this.props.dispatcher,
              failureReason: this.state.failureReason });
          }
        case ROOM_STATES.ENDED:
          {
            // When conversation ended we either display a feedback form or
            // close the window. This is decided in the AppControllerView.
            return null;
          }
        default:
          {
            return React.createElement(
              "div",
              { className: "room-conversation-wrapper desktop-room-wrapper",
                onContextMenu: this.handleContextMenu },
              React.createElement(
                sharedViews.MediaLayoutView,
                {
                  cursorStore: this.props.cursorStore,
                  dispatcher: this.props.dispatcher,
                  displayScreenShare: false,
                  isLocalLoading: this._isLocalLoading(),
                  isRemoteLoading: this._isRemoteLoading(),
                  isScreenShareLoading: false,
                  localPosterUrl: this.props.localPosterUrl,
                  localSrcMediaElement: this.state.localSrcMediaElement,
                  localVideoMuted: this.state.videoMuted,
                  matchMedia: this.state.matchMedia || window.matchMedia.bind(window),
                  remotePosterUrl: this.props.remotePosterUrl,
                  remoteSrcMediaElement: this.state.remoteSrcMediaElement,
                  renderRemoteVideo: this.shouldRenderRemoteVideo(),
                  screenShareMediaElement: this.state.screenShareMediaElement,
                  screenSharePosterUrl: null,
                  showInitialContext: false,
                  showMediaWait: false,
                  showTile: false },
                React.createElement(sharedViews.ConversationToolbar, {
                  audio: { enabled: !this.state.audioMuted, visible: true },
                  dispatcher: this.props.dispatcher,
                  hangup: this.leaveRoom,
                  showHangup: this.props.chatWindowDetached,
                  video: { enabled: !this.state.videoMuted, visible: true } }),
                React.createElement(sharedDesktopViews.SharePanelView, {
                  dispatcher: this.props.dispatcher,
                  error: this.state.error,
                  facebookEnabled: this.props.facebookEnabled,
                  locationForMetrics: "conversation",
                  roomData: roomData,
                  show: shouldRenderInvitationOverlay,
                  socialShareProviders: this.state.socialShareProviders })
              )
            );
          }
      }
    }
  });

  return {
    ActiveRoomStoreMixin: ActiveRoomStoreMixin,
    FailureInfoView: FailureInfoView,
    RoomFailureView: RoomFailureView,
    DesktopRoomConversationView: DesktopRoomConversationView
  };
}(document.mozL10n || navigator.mozL10n);
