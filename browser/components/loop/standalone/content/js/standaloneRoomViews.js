/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

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

  var StandaloneRoomInfoArea = React.createClass({displayName: "StandaloneRoomInfoArea",
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
          React.createElement("a", {href: loop.config.learnMoreUrl, className: "btn btn-info"}, 
            mozL10n.get("rooms_room_full_call_to_action_label", {
              clientShortname: mozL10n.get("clientShortname2")
            })
          )
        );
      }
      return (
        React.createElement("a", {href: loop.config.downloadFirefoxUrl, className: "btn btn-info"}, 
          mozL10n.get("rooms_room_full_call_to_action_nonFx_label", {
            brandShortname: mozL10n.get("brandShortname")
          })
        )
      );
    },

    /**
     * @return String An appropriate string according to the failureReason.
     */
    _getFailureString: function() {
      switch(this.props.failureReason) {
        case FAILURE_DETAILS.MEDIA_DENIED:
        // XXX Bug 1166824 should provide a better string for this.
        case FAILURE_DETAILS.NO_MEDIA:
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
            React.createElement("div", {className: "room-inner-info-area"}, 
              React.createElement("button", {className: "btn btn-join btn-info", 
                      onClick: this.props.joinRoom}, 
                mozL10n.get("rooms_room_join_label")
              )
            )
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
            React.createElement("div", {className: "room-inner-info-area"}, 
              React.createElement("p", {className: promptMediaMessageClasses}, 
                msg
              )
            )
          );
        }
        case ROOM_STATES.JOINING:
        case ROOM_STATES.JOINED:
        case ROOM_STATES.SESSION_CONNECTED: {
          return (
            React.createElement("div", {className: "room-inner-info-area"}, 
              React.createElement("p", {className: "empty-room-message"}, 
                mozL10n.get("rooms_only_occupant_label")
              )
            )
          );
        }
        case ROOM_STATES.FULL: {
          return (
            React.createElement("div", {className: "room-inner-info-area"}, 
              React.createElement("p", {className: "full-room-message"}, 
                mozL10n.get("rooms_room_full_label")
              ), 
              React.createElement("p", null, this._renderCallToActionLink())
            )
          );
        }
        case ROOM_STATES.ENDED: {
          if (this.props.roomUsed) {
            return (
              React.createElement("div", {className: "ended-conversation"}, 
                React.createElement(sharedViews.FeedbackView, {
                  onAfterFeedbackReceived: this.onFeedbackSent, 
                  noCloseText: true}
                )
              )
            );
          }

          // In case the room was not used (no one was here), we
          // bypass the feedback form.
          this.onFeedbackSent();
          return null;
        }
        case ROOM_STATES.FAILED: {
          return (
            React.createElement("div", {className: "room-inner-info-area"}, 
              React.createElement("p", {className: "failed-room-message"}, 
                this._getFailureString()
              ), 
              React.createElement("button", {className: "btn btn-join btn-info", 
                      onClick: this.props.joinRoom}, 
                mozL10n.get("retry_call_button")
              )
            )
          );
        }
        default: {
          return null;
        }
      }
    }
  });

  var StandaloneRoomHeader = React.createClass({displayName: "StandaloneRoomHeader",
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
        React.createElement("header", null, 
          React.createElement("h1", null, mozL10n.get("clientShortname2")), 
          React.createElement("a", {href: loop.config.generalSupportUrl, 
             onClick: this.recordClick, 
             rel: "noreferrer", 
             target: "_blank"}, 
            React.createElement("i", {className: "icon icon-help"})
          )
        )
      );
    }
  });

  var StandaloneRoomFooter = React.createClass({displayName: "StandaloneRoomFooter",
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
          React.createElement("a", {href: loop.config.legalWebsiteUrl, rel: "noreferrer", target: "_blank"}, 
            mozL10n.get("terms_of_use_link_text")
          )
        ),
        "privacy_notice_url": React.renderToStaticMarkup(
          React.createElement("a", {href: loop.config.privacyWebsiteUrl, rel: "noreferrer", target: "_blank"}, 
            mozL10n.get("privacy_notice_link_text")
          )
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
        React.createElement("footer", {className: "rooms-footer"}, 
          React.createElement("div", {className: "footer-logo"}), 
          React.createElement("p", {dangerouslySetInnerHTML: {__html: this._getContent()}, 
             onClick: this.recordClick})
        )
      );
    }
  });

  var StandaloneRoomView = React.createClass({displayName: "StandaloneRoomView",
    mixins: [
      Backbone.Events,
      sharedMixins.MediaSetupMixin,
      sharedMixins.RoomsAudioMixin,
      loop.store.StoreMixin("activeRoomStore")
    ],

    propTypes: {
      activeRoomStore: React.PropTypes.oneOfType([
        React.PropTypes.instanceOf(loop.store.ActiveRoomStore),
        React.PropTypes.instanceOf(loop.store.FxOSActiveRoomStore)
      ]).isRequired,
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      isFirefox: React.PropTypes.bool.isRequired,
      // The poster URLs are for UI-showcase testing and development
      localPosterUrl: React.PropTypes.string,
      remotePosterUrl: React.PropTypes.string,
      screenSharePosterUrl: React.PropTypes.string
    },

    getInitialState: function() {
      var storeState = this.props.activeRoomStore.getStoreState();
      return _.extend({}, storeState, {
        // Used by the UI showcase.
        roomState: this.props.roomState || storeState.roomState
      });
    },

    componentDidMount: function() {
      // Adding a class to the document body element from here to ease styling it.
      document.body.classList.add("is-standalone-room");
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
          publisherConfig: this.getDefaultPublisherConfig({publishVideo: true})
        }));
      }

      // UX don't want to surface these errors (as they would imply the user
      // needs to do something to fix them, when if they're having a conversation
      // they just need to connect). However, we do want there to be somewhere to
      // find reasonably easily, in case there's issues raised.
      if (!this.state.roomInfoFailure && nextState.roomInfoFailure) {
        if (nextState.roomInfoFailure === ROOM_INFO_FAILURES.WEB_CRYPTO_UNSUPPORTED) {
          console.error(mozL10n.get("room_information_failure_unsupported_browser"));
        } else {
          console.error(mozL10n.get("room_information_failure_not_available"));
        }
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
     * Checks if current room is active.
     *
     * @return {Boolean}
     */
    _roomIsActive: function() {
      return this.state.roomState === ROOM_STATES.JOINED            ||
             this.state.roomState === ROOM_STATES.SESSION_CONNECTED ||
             this.state.roomState === ROOM_STATES.HAS_PARTICIPANTS;
    },

    /**
     * Works out if remote video should be rended or not, depending on the
     * room state and other flags.
     *
     * @return {Boolean} True if remote video should be rended.
     */
    shouldRenderRemoteVideo: function() {
      switch(this.state.roomState) {
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
        case ROOM_STATES.INIT:
        case ROOM_STATES.JOINING:
        case ROOM_STATES.SESSION_CONNECTED:
        case ROOM_STATES.JOINED:
        case ROOM_STATES.MEDIA_WAIT:
          // this case is so that we don't show an avatar while waiting for
          // the other party to connect
          return true;

        case ROOM_STATES.CLOSING:
          // the other person has shown up, so we don't want to show an avatar
          return true;

        default:
          console.warn("StandaloneRoomView.shouldRenderRemoteVideo:" +
            " unexpected roomState: ", this.state.roomState);
          return true;

      }
    },

    render: function() {
      var displayScreenShare = this.state.receivingScreenShare ||
        this.props.screenSharePosterUrl;

      var remoteStreamClasses = React.addons.classSet({
        "remote": true,
        "focus-stream": !displayScreenShare
      });

      var screenShareStreamClasses = React.addons.classSet({
        "screen": true,
        "focus-stream": displayScreenShare
      });

      var mediaWrapperClasses = React.addons.classSet({
        "media-wrapper": true,
        "receiving-screen-share": displayScreenShare,
        "showing-local-streams": this.state.localSrcVideoObject ||
          this.props.localPosterUrl
      });

      return (
        React.createElement("div", {className: "room-conversation-wrapper"}, 
          React.createElement("div", {className: "beta-logo"}), 
          React.createElement(StandaloneRoomHeader, {dispatcher: this.props.dispatcher}), 
          React.createElement(StandaloneRoomInfoArea, {roomState: this.state.roomState, 
                                  failureReason: this.state.failureReason, 
                                  joinRoom: this.joinRoom, 
                                  isFirefox: this.props.isFirefox, 
                                  activeRoomStore: this.props.activeRoomStore, 
                                  roomUsed: this.state.used}), 
          React.createElement("div", {className: "media-layout"}, 
            React.createElement("div", {className: mediaWrapperClasses}, 
              React.createElement("span", {className: "self-view-hidden-message"}, 
                mozL10n.get("self_view_hidden_message")
              ), 
              React.createElement("div", {className: remoteStreamClasses}, 
                React.createElement(sharedViews.MediaView, {displayAvatar: !this.shouldRenderRemoteVideo(), 
                  posterUrl: this.props.remotePosterUrl, 
                  mediaType: "remote", 
                  srcVideoObject: this.state.remoteSrcVideoObject})
              ), 
              React.createElement("div", {className: screenShareStreamClasses}, 
                React.createElement(sharedViews.MediaView, {displayAvatar: false, 
                  posterUrl: this.props.screenSharePosterUrl, 
                  mediaType: "screen-share", 
                  srcVideoObject: this.state.screenShareVideoObject})
              ), 
              React.createElement(sharedViews.TextChatView, {
                dispatcher: this.props.dispatcher, 
                showAlways: true, 
                showRoomName: true}), 
              React.createElement("div", {className: "local"}, 
                React.createElement(sharedViews.MediaView, {displayAvatar: this.state.videoMuted, 
                  posterUrl: this.props.localPosterUrl, 
                  mediaType: "local", 
                  srcVideoObject: this.state.localSrcVideoObject})
              )
            ), 
            React.createElement(sharedViews.ConversationToolbar, {
              dispatcher: this.props.dispatcher, 
              video: {enabled: !this.state.videoMuted,
                      visible: this._roomIsActive()}, 
              audio: {enabled: !this.state.audioMuted,
                      visible: this._roomIsActive()}, 
              publishStream: this.publishStream, 
              hangup: this.leaveRoom, 
              hangupButtonLabel: mozL10n.get("rooms_leave_button_label"), 
              enableHangup: this._roomIsActive()})
          ), 
          React.createElement(loop.fxOSMarketplaceViews.FxOSHiddenMarketplaceView, {
            marketplaceSrc: this.state.marketplaceSrc, 
            onMarketplaceMessage: this.state.onMarketplaceMessage}), 
          React.createElement(StandaloneRoomFooter, {dispatcher: this.props.dispatcher})
        )
      );
    }
  });

  return {
    StandaloneRoomFooter: StandaloneRoomFooter,
    StandaloneRoomHeader: StandaloneRoomHeader,
    StandaloneRoomView: StandaloneRoomView
  };
})(navigator.mozL10n);
