/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true, React */
/* jshint newcap:false, maxlen:false */

var loop = loop || {};
loop.standaloneRoomViews = (function(mozL10n) {
  "use strict";

  var FAILURE_DETAILS = loop.shared.utils.FAILURE_DETAILS;
  var ROOM_INFO_FAILURES = loop.shared.utils.ROOM_INFO_FAILURES;
  var ROOM_STATES = loop.store.ROOM_STATES;
  var sharedActions = loop.shared.actions;
  var sharedMixins = loop.shared.mixins;
  var sharedUtils = loop.shared.utils;
  var sharedViews = loop.shared.views;

  var StandaloneRoomInfoArea = React.createClass({
    propTypes: {
      isFirefox: React.PropTypes.bool.isRequired,
      activeRoomStore: React.PropTypes.oneOfType([
        React.PropTypes.instanceOf(loop.store.ActiveRoomStore),
        React.PropTypes.instanceOf(loop.store.FxOSActiveRoomStore)
      ]).isRequired
    },

    onFeedbackSent: function() {
      // We pass a tick to prevent React warnings regarding nested updates.
      setTimeout(function() {
        this.props.activeRoomStore.dispatchAction(new sharedActions.FeedbackComplete());
      }.bind(this));
    },

    _renderCallToActionLink: function() {
      if (this.props.isFirefox) {
        return (
          <a href={loop.config.learnMoreUrl} className="btn btn-info">
            {mozL10n.get("rooms_room_full_call_to_action_label", {
              clientShortname: mozL10n.get("clientShortname2")
            })}
          </a>
        );
      }
      return (
        <a href={loop.config.downloadFirefoxUrl} className="btn btn-info">
          {mozL10n.get("rooms_room_full_call_to_action_nonFx_label", {
            brandShortname: mozL10n.get("brandShortname")
          })}
        </a>
      );
    },

    /**
     * @return String An appropriate string according to the failureReason.
     */
    _getFailureString: function() {
      switch(this.props.failureReason) {
        case FAILURE_DETAILS.MEDIA_DENIED:
          return mozL10n.get("rooms_media_denied_message");
        case FAILURE_DETAILS.EXPIRED_OR_INVALID:
          return mozL10n.get("rooms_unavailable_notification_message");
        default:
          return mozL10n.get("status_error");
      }
    },

    render: function() {
      switch(this.props.roomState) {
        case ROOM_STATES.INIT:
        case ROOM_STATES.READY: {
          // XXX: In ENDED state, we should rather display the feedback form.
          return (
            <div className="room-inner-info-area">
              <button className="btn btn-join btn-info"
                      onClick={this.props.joinRoom}>
                {mozL10n.get("rooms_room_join_label")}
              </button>
            </div>
          );
        }
        case ROOM_STATES.MEDIA_WAIT: {
          var msg = mozL10n.get("call_progress_getting_media_description",
                                {clientShortname: mozL10n.get("clientShortname2")});
          var utils = loop.shared.utils;
          var isChrome = utils.isChrome(navigator.userAgent);
          var isFirefox = utils.isFirefox(navigator.userAgent);
          var isOpera = utils.isOpera(navigator.userAgent);
          var promptMediaMessageClasses = React.addons.classSet({
            "prompt-media-message": true,
            "chrome": isChrome,
            "firefox": isFirefox,
            "opera": isOpera,
            "other": !isChrome && !isFirefox && !isOpera
          });
          return (
            <div className="room-inner-info-area">
              <p className={promptMediaMessageClasses}>
                {msg}
              </p>
            </div>
          );
        }
        case ROOM_STATES.JOINING:
        case ROOM_STATES.JOINED:
        case ROOM_STATES.SESSION_CONNECTED: {
          return (
            <div className="room-inner-info-area">
              <p className="empty-room-message">
                {mozL10n.get("rooms_only_occupant_label")}
              </p>
            </div>
          );
        }
        case ROOM_STATES.FULL: {
          return (
            <div className="room-inner-info-area">
              <p className="full-room-message">
                {mozL10n.get("rooms_room_full_label")}
              </p>
              <p>{this._renderCallToActionLink()}</p>
            </div>
          );
        }
        case ROOM_STATES.ENDED: {
          if (this.props.roomUsed)
            return (
              <div className="ended-conversation">
                <sharedViews.FeedbackView
                  onAfterFeedbackReceived={this.onFeedbackSent}
                />
              </div>
            );

          // In case the room was not used (no one was here), we
          // bypass the feedback form.
          this.onFeedbackSent();
          return null;
        }
        case ROOM_STATES.FAILED: {
          return (
            <div className="room-inner-info-area">
              <p className="failed-room-message">
                {this._getFailureString()}
              </p>
              <button className="btn btn-join btn-info"
                      onClick={this.props.joinRoom}>
                {mozL10n.get("retry_call_button")}
              </button>
            </div>
          );
        }
        default: {
          return null;
        }
      }
    }
  });

  var StandaloneRoomHeader = React.createClass({
    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired
    },

    recordClick: function() {
      this.props.dispatcher.dispatch(new sharedActions.RecordClick({
        linkInfo: "Support link click"
      }));
    },

    render: function() {
      return (
        <header>
          <h1>{mozL10n.get("clientShortname2")}</h1>
          <a href={loop.config.generalSupportUrl}
             onClick={this.recordClick}
             target="_blank">
            <i className="icon icon-help"></i>
          </a>
        </header>
      );
    }
  });

  var StandaloneRoomFooter = React.createClass({
    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired
    },

    _getContent: function() {
      // We use this technique of static markup as it means we get
      // just one overall string for L10n to define the structure of
      // the whole item.
      return mozL10n.get("legal_text_and_links", {
        "clientShortname": mozL10n.get("clientShortname2"),
        "terms_of_use_url": React.renderToStaticMarkup(
          <a href={loop.config.legalWebsiteUrl} target="_blank">
            {mozL10n.get("terms_of_use_link_text")}
          </a>
        ),
        "privacy_notice_url": React.renderToStaticMarkup(
          <a href={loop.config.privacyWebsiteUrl} target="_blank">
            {mozL10n.get("privacy_notice_link_text")}
          </a>
        )
      });
    },

    recordClick: function(event) {
      // Check for valid href, as this is clicking on the paragraph -
      // so the user may be clicking on the text rather than the link.
      if (event.target && event.target.href) {
        this.props.dispatcher.dispatch(new sharedActions.RecordClick({
          linkInfo: event.target.href
        }));
      }
    },

    render: function() {
      return (
        <footer>
          <p dangerouslySetInnerHTML={{__html: this._getContent()}}
             onClick={this.recordClick}></p>
          <div className="footer-logo" />
        </footer>
      );
    }
  });

  var StandaloneRoomContextItem = React.createClass({
    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      receivingScreenShare: React.PropTypes.bool,
      roomContextUrl: React.PropTypes.object
    },

    recordClick: function() {
      this.props.dispatcher.dispatch(new sharedActions.RecordClick({
        linkInfo: "Shared URL"
      }));
    },

    render: function() {
      if (!this.props.roomContextUrl ||
          !this.props.roomContextUrl.location) {
        return null;
      }

      var locationInfo = sharedUtils.formatURL(this.props.roomContextUrl.location);
      if (!locationInfo) {
        return null;
      }

      var cx = React.addons.classSet;

      var classes = cx({
        "standalone-context-url": true,
        "screen-share-active": this.props.receivingScreenShare
      });

      return (
        <div className={classes}>
            <img src={this.props.roomContextUrl.thumbnail} />
          <div className="standalone-context-url-description-wrapper">
            {this.props.roomContextUrl.description}
            <br /><a href={locationInfo.location}
                     onClick={this.recordClick}
                     target="_blank"
                     title={locationInfo.location}>{locationInfo.hostname}</a>
          </div>
        </div>
      );
    }
  });

  var StandaloneRoomContextView = React.createClass({
    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      receivingScreenShare: React.PropTypes.bool.isRequired,
      roomContextUrls: React.PropTypes.array,
      roomName: React.PropTypes.string,
      roomInfoFailure: React.PropTypes.string
    },

    getInitialState: function() {
      return {
        failureLogged: false
      };
    },

    _logFailure: function(message) {
      if (!this.state.failureLogged) {
        console.error(mozL10n.get(message));
        this.state.failureLogged = true;
      }
    },

    render: function() {
      // For failures, we currently just log the messages - UX doesn't want them
      // displayed on primary UI at the moment.
      if (this.props.roomInfoFailure === ROOM_INFO_FAILURES.WEB_CRYPTO_UNSUPPORTED) {
        this._logFailure("room_information_failure_unsupported_browser");
        return null;
      } else if (this.props.roomInfoFailure) {
        this._logFailure("room_information_failure_not_available");
        return null;
      }

      // We only support one item in the context Urls array for now.
      var roomContextUrl = (this.props.roomContextUrls &&
                            this.props.roomContextUrls.length > 0) ?
                            this.props.roomContextUrls[0] : null;
      return (
        <div className="standalone-room-info">
          <h2 className="room-name">{this.props.roomName}</h2>
          <StandaloneRoomContextItem
            dispatcher={this.props.dispatcher}
            receivingScreenShare={this.props.receivingScreenShare}
            roomContextUrl={roomContextUrl} />
        </div>
      );
    }
  });

  var StandaloneRoomView = React.createClass({
    mixins: [
      Backbone.Events,
      sharedMixins.MediaSetupMixin,
      sharedMixins.RoomsAudioMixin
    ],

    propTypes: {
      activeRoomStore: React.PropTypes.oneOfType([
        React.PropTypes.instanceOf(loop.store.ActiveRoomStore),
        React.PropTypes.instanceOf(loop.store.FxOSActiveRoomStore)
      ]).isRequired,
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      isFirefox: React.PropTypes.bool.isRequired
    },

    getInitialState: function() {
      var storeState = this.props.activeRoomStore.getStoreState();
      return _.extend({}, storeState, {
        // Used by the UI showcase.
        roomState: this.props.roomState || storeState.roomState
      });
    },

    componentWillMount: function() {
      this.listenTo(this.props.activeRoomStore, "change",
                    this._onActiveRoomStateChanged);
    },

    /**
     * Handles a "change" event on the roomStore, and updates this.state
     * to match the store.
     *
     * @private
     */
    _onActiveRoomStateChanged: function() {
      var state = this.props.activeRoomStore.getStoreState();
      this.updateVideoDimensions(state.localVideoDimensions, state.remoteVideoDimensions);
      this.setState(state);
    },

    componentDidMount: function() {
      // Adding a class to the document body element from here to ease styling it.
      document.body.classList.add("is-standalone-room");
    },

    componentWillUnmount: function() {
      this.stopListening(this.props.activeRoomStore);
    },

    /**
     * Watches for when we transition to MEDIA_WAIT room state, so we can request
     * user media access.
     *
     * @param  {Object} nextProps (Unused)
     * @param  {Object} nextState Next state object.
     */
    componentWillUpdate: function(nextProps, nextState) {
      if (this.state.roomState !== ROOM_STATES.MEDIA_WAIT &&
          nextState.roomState === ROOM_STATES.MEDIA_WAIT) {
        this.props.dispatcher.dispatch(new sharedActions.SetupStreamElements({
          publisherConfig: this.getDefaultPublisherConfig({publishVideo: true}),
          getLocalElementFunc: this._getElement.bind(this, ".local"),
          getRemoteElementFunc: this._getElement.bind(this, ".remote"),
          getScreenShareElementFunc: this._getElement.bind(this, ".screen")
        }));
      }

      if (this.state.roomState !== ROOM_STATES.JOINED &&
          nextState.roomState === ROOM_STATES.JOINED) {
        // This forces the video size to update - creating the publisher
        // first, and then connecting to the session doesn't seem to set the
        // initial size correctly.
        this.updateVideoContainer();
      }

      if (nextState.roomState === ROOM_STATES.INIT ||
          nextState.roomState === ROOM_STATES.GATHER ||
          nextState.roomState === ROOM_STATES.READY) {
        this.resetDimensionsCache();
      }

      // When screen sharing stops.
      if (this.state.receivingScreenShare && !nextState.receivingScreenShare) {
        // Remove the custom screenshare styles on the remote camera.
        var node = this._getElement(".remote");
        node.removeAttribute("style");

        // Force the video sizes to update.
        this.updateVideoContainer();
      }
    },

    joinRoom: function() {
      this.props.dispatcher.dispatch(new sharedActions.JoinRoom());
    },

    leaveRoom: function() {
      this.props.dispatcher.dispatch(new sharedActions.LeaveRoom());
    },

    /**
     * Toggles streaming status for a given stream type.
     *
     * @param  {String}  type     Stream type ("audio" or "video").
     * @param  {Boolean} enabled  Enabled stream flag.
     */
    publishStream: function(type, enabled) {
      this.props.dispatcher.dispatch(new sharedActions.SetMute({
        type: type,
        enabled: enabled
      }));
    },

    /**
     * Specifically updates the local camera stream size and position, depending
     * on the size and position of the remote video stream.
     * This method gets called from `updateVideoContainer`, which is defined in
     * the `MediaSetupMixin`.
     *
     * @param  {Object} ratio Aspect ratio of the local camera stream
     */
    updateLocalCameraPosition: function(ratio) {
      // The local stream is a quarter of the remote stream.
      var LOCAL_STREAM_SIZE = 0.25;
      // The local stream overlaps the remote stream by a quarter of the local stream.
      var LOCAL_STREAM_OVERLAP = 0.25;
      // The minimum size of video height/width allowed by the sdk css.
      var SDK_MIN_SIZE = 48;

      var node = this._getElement(".local");
      var targetWidth;

      node.style.right = "auto";
      if (window.matchMedia && window.matchMedia("screen and (max-width:640px)").matches) {
        // For reduced screen widths, we just go for a fixed size and no overlap.
        targetWidth = 180;
        node.style.width = (targetWidth * ratio.width) + "px";
        node.style.height = (targetWidth * ratio.height) + "px";
        node.style.left = "auto";
      } else {
        // The local camera view should be a quarter of the size of the remote stream
        // and positioned to overlap with the remote stream at a quarter of its width.

        // Now position the local camera view correctly with respect to the remote
        // video stream or the screen share stream.
        var remoteVideoDimensions = this.getRemoteVideoDimensions(
          this.state.receivingScreenShare ? "screen" : "camera");

        targetWidth = remoteVideoDimensions.streamWidth * LOCAL_STREAM_SIZE;

        var realWidth = targetWidth * ratio.width;
        var realHeight = targetWidth * ratio.height;

        // If we've hit the min size limits, then limit at the minimum.
        if (realWidth < SDK_MIN_SIZE) {
          realWidth = SDK_MIN_SIZE;
          realHeight = realWidth / ratio.width * ratio.height;
        }
        if (realHeight < SDK_MIN_SIZE) {
          realHeight = SDK_MIN_SIZE;
          realWidth = realHeight / ratio.height * ratio.width;
        }

        var offsetX = (remoteVideoDimensions.streamWidth + remoteVideoDimensions.offsetX);
        // The horizontal offset of the stream, and the width of the resulting
        // pillarbox, is determined by the height exponent of the aspect ratio.
        // Therefore we multiply the width of the local camera view by the height
        // ratio.
        node.style.left = (offsetX - (realWidth * LOCAL_STREAM_OVERLAP)) + "px";
        node.style.width = realWidth + "px";
        node.style.height = realHeight + "px";
      }
    },

    /**
     * Specifically updates the remote camera stream size and position, if
     * a screen share is being received. It is slaved from the position of the
     * local stream.
     * This method gets called from `updateVideoContainer`, which is defined in
     * the `MediaSetupMixin`.
     *
     * @param  {Object} ratio Aspect ratio of the remote camera stream
     */
    updateRemoteCameraPosition: function(ratio) {
      // Nothing to do for screenshare
      if (!this.state.receivingScreenShare) {
        return;
      }
      // XXX For the time being, if we're a narrow screen, aka mobile, we don't display
      // the remote media (bug 1133534).
      if (window.matchMedia && window.matchMedia("screen and (max-width:640px)").matches) {
        return;
      }

      // 10px separation between the two streams.
      var LOCAL_REMOTE_SEPARATION = 10;

      var node = this._getElement(".remote");
      var localNode = this._getElement(".local");

      // Match the width to the local video.
      node.style.width = localNode.offsetWidth + "px";

      // The height is then determined from the aspect ratio
      var height = ((localNode.offsetWidth / ratio.width) * ratio.height);
      node.style.height = height + "px";

      node.style.right = "auto";
      node.style.bottom = "auto";

      // Now position the local camera view correctly with respect to the remote
      // video stream.

      // The top is measured from the top of the element down the screen,
      // so subtract the height of the video and the separation distance.
      node.style.top = (localNode.offsetTop - height - LOCAL_REMOTE_SEPARATION) + "px";

      // Match the left-hand sides.
      node.style.left = localNode.offsetLeft + "px";
    },

    /**
     * Checks if current room is active.
     *
     * @return {Boolean}
     */
    _roomIsActive: function() {
      return this.state.roomState === ROOM_STATES.JOINED            ||
             this.state.roomState === ROOM_STATES.SESSION_CONNECTED ||
             this.state.roomState === ROOM_STATES.HAS_PARTICIPANTS;
    },

    render: function() {
      var localStreamClasses = React.addons.classSet({
        hide: !this._roomIsActive(),
        local: true,
        "local-stream": true,
        "local-stream-audio": this.state.videoMuted
      });

      var remoteStreamClasses = React.addons.classSet({
        "video_inner": true,
        "remote": true,
        "focus-stream": !this.state.receivingScreenShare,
        "remote-inset-stream": this.state.receivingScreenShare
      });

      var screenShareStreamClasses = React.addons.classSet({
        "screen": true,
        "focus-stream": this.state.receivingScreenShare,
        hide: !this.state.receivingScreenShare
      });

      return (
        <div className="room-conversation-wrapper">
          <div className="beta-logo" />
          <StandaloneRoomHeader dispatcher={this.props.dispatcher} />
          <StandaloneRoomInfoArea roomState={this.state.roomState}
                                  failureReason={this.state.failureReason}
                                  joinRoom={this.joinRoom}
                                  isFirefox={this.props.isFirefox}
                                  activeRoomStore={this.props.activeRoomStore}
                                  roomUsed={this.state.used} />
          <div className="video-layout-wrapper">
            <div className="conversation room-conversation">
              <StandaloneRoomContextView
                dispatcher={this.props.dispatcher}
                receivingScreenShare={this.state.receivingScreenShare}
                roomContextUrls={this.state.roomContextUrls}
                roomName={this.state.roomName}
                roomInfoFailure={this.state.roomInfoFailure} />
              <div className="media nested">
                <span className="self-view-hidden-message">
                  {mozL10n.get("self_view_hidden_message")}
                </span>
                <div className="video_wrapper remote_wrapper">
                  <div className={remoteStreamClasses}></div>
                  <div className={screenShareStreamClasses}></div>
                </div>
                <div className={localStreamClasses}></div>
              </div>
              <sharedViews.ConversationToolbar
                dispatcher={this.props.dispatcher}
                video={{enabled: !this.state.videoMuted,
                        visible: this._roomIsActive()}}
                audio={{enabled: !this.state.audioMuted,
                        visible: this._roomIsActive()}}
                publishStream={this.publishStream}
                hangup={this.leaveRoom}
                hangupButtonLabel={mozL10n.get("rooms_leave_button_label")}
                enableHangup={this._roomIsActive()} />
            </div>
          </div>
          <loop.fxOSMarketplaceViews.FxOSHiddenMarketplaceView
            marketplaceSrc={this.state.marketplaceSrc}
            onMarketplaceMessage={this.state.onMarketplaceMessage} />
          <StandaloneRoomFooter dispatcher={this.props.dispatcher} />
        </div>
      );
    }
  });

  return {
    StandaloneRoomContextView: StandaloneRoomContextView,
    StandaloneRoomFooter: StandaloneRoomFooter,
    StandaloneRoomHeader: StandaloneRoomHeader,
    StandaloneRoomView: StandaloneRoomView
  };
})(navigator.mozL10n);
