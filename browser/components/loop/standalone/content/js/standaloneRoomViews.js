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
  var ROOM_STATES = loop.store.ROOM_STATES;
  var sharedActions = loop.shared.actions;
  var sharedMixins = loop.shared.mixins;
  var sharedViews = loop.shared.views;

  var StandaloneRoomInfoArea = React.createClass({displayName: "StandaloneRoomInfoArea",
    propTypes: {
      helper: React.PropTypes.instanceOf(loop.shared.utils.Helper).isRequired,
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
      if (this.props.helper.isFirefox(navigator.userAgent)) {
        return (
          React.createElement("a", {href: loop.config.learnMoreUrl, className: "btn btn-info"}, 
            mozL10n.get("rooms_room_full_call_to_action_label", {
              clientShortname: mozL10n.get("clientShortname2")
            })
          )
        );
      }
      return (
        React.createElement("a", {href: loop.config.brandWebsiteUrl, className: "btn btn-info"}, 
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
          // XXX Bug 1047040 will add images to help prompt the user.
          return (
            React.createElement("div", {className: "room-inner-info-area"}, 
              React.createElement("p", {className: "prompt-media-message"}, 
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
          if (this.props.roomUsed)
            return (
              React.createElement("div", {className: "ended-conversation"}, 
                React.createElement(sharedViews.FeedbackView, {
                  onAfterFeedbackReceived: this.onFeedbackSent}
                )
              )
            );

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
    render: function() {
      return (
        React.createElement("header", null, 
          React.createElement("h1", null, mozL10n.get("clientShortname2")), 
          React.createElement("a", {target: "_blank", href: loop.config.generalSupportUrl}, 
            React.createElement("i", {className: "icon icon-help"})
          )
        )
      );
    }
  });

  var StandaloneRoomFooter = React.createClass({displayName: "StandaloneRoomFooter",
    _getContent: function() {
      return mozL10n.get("legal_text_and_links", {
        "clientShortname": mozL10n.get("clientShortname2"),
        "terms_of_use_url": React.renderToStaticMarkup(
          React.createElement("a", {href: loop.config.legalWebsiteUrl, target: "_blank"}, 
            mozL10n.get("terms_of_use_link_text")
          )
        ),
        "privacy_notice_url": React.renderToStaticMarkup(
          React.createElement("a", {href: loop.config.privacyWebsiteUrl, target: "_blank"}, 
            mozL10n.get("privacy_notice_link_text")
          )
        ),
      });
    },

    render: function() {
      return (
        React.createElement("footer", null, 
          React.createElement("p", {dangerouslySetInnerHTML: {__html: this._getContent()}}), 
          React.createElement("div", {className: "footer-logo"})
        )
      );
    }
  });

  var StandaloneRoomView = React.createClass({displayName: "StandaloneRoomView",
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
      helper: React.PropTypes.instanceOf(loop.shared.utils.Helper).isRequired
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
      this.setState(this.props.activeRoomStore.getStoreState());
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
          getRemoteElementFunc: this._getElement.bind(this, ".remote")
        }));
      }

      if (this.state.roomState !== ROOM_STATES.JOINED &&
          nextState.roomState === ROOM_STATES.JOINED) {
        // This forces the video size to update - creating the publisher
        // first, and then connecting to the session doesn't seem to set the
        // initial size correctly.
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

      return (
        React.createElement("div", {className: "room-conversation-wrapper"}, 
          React.createElement("div", {className: "beta-logo"}), 
          React.createElement(StandaloneRoomHeader, null), 
          React.createElement(StandaloneRoomInfoArea, {roomState: this.state.roomState, 
                                  failureReason: this.state.failureReason, 
                                  joinRoom: this.joinRoom, 
                                  helper: this.props.helper, 
                                  activeRoomStore: this.props.activeRoomStore, 
                                  roomUsed: this.state.used}), 
          React.createElement("div", {className: "video-layout-wrapper"}, 
            React.createElement("div", {className: "conversation room-conversation"}, 
              React.createElement("h2", {className: "room-name"}, this.state.roomName), 
              React.createElement("div", {className: "media nested"}, 
                React.createElement("span", {className: "self-view-hidden-message"}, 
                  mozL10n.get("self_view_hidden_message")
                ), 
                React.createElement("div", {className: "video_wrapper remote_wrapper"}, 
                  React.createElement("div", {className: "video_inner remote"})
                ), 
                React.createElement("div", {className: localStreamClasses})
              ), 
              React.createElement(sharedViews.ConversationToolbar, {
                video: {enabled: !this.state.videoMuted,
                        visible: this._roomIsActive()}, 
                audio: {enabled: !this.state.audioMuted,
                        visible: this._roomIsActive()}, 
                publishStream: this.publishStream, 
                hangup: this.leaveRoom, 
                hangupButtonLabel: mozL10n.get("rooms_leave_button_label"), 
                enableHangup: this._roomIsActive()})
            )
          ), 
          React.createElement(loop.fxOSMarketplaceViews.FxOSHiddenMarketplaceView, {
            marketplaceSrc: this.state.marketplaceSrc, 
            onMarketplaceMessage: this.state.onMarketplaceMessage}), 
          React.createElement(StandaloneRoomFooter, null)
        )
      );
    }
  });

  return {
    StandaloneRoomView: StandaloneRoomView
  };
})(navigator.mozL10n);
