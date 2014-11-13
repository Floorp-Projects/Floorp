/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true, React */
/* jshint newcap:false, maxlen:false */

var loop = loop || {};
loop.standaloneRoomViews = (function(mozL10n) {
  "use strict";

  var FAILURE_REASONS = loop.shared.utils.FAILURE_REASONS;
  var ROOM_STATES = loop.store.ROOM_STATES;
  var sharedActions = loop.shared.actions;
  var sharedMixins = loop.shared.mixins;
  var sharedViews = loop.shared.views;

  var StandaloneRoomInfoArea = React.createClass({displayName: 'StandaloneRoomInfoArea',
    propTypes: {
      helper: React.PropTypes.instanceOf(loop.shared.utils.Helper).isRequired
    },

    _renderCallToActionLink: function() {
      if (this.props.helper.isFirefox(navigator.userAgent)) {
        return (
          React.DOM.a({href: loop.config.learnMoreUrl, className: "btn btn-info"}, 
            mozL10n.get("rooms_room_full_call_to_action_label", {
              clientShortname: mozL10n.get("clientShortname2")
            })
          )
        );
      }
      return (
        React.DOM.a({href: loop.config.brandWebsiteUrl, className: "btn btn-info"}, 
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
        case FAILURE_REASONS.MEDIA_DENIED:
          return mozL10n.get("rooms_media_denied_message");
        case FAILURE_REASONS.EXPIRED_OR_INVALID:
          return mozL10n.get("rooms_unavailable_notification_message");
        default:
          return mozL10n.get("status_error");
      };
    },

    _renderContent: function() {
      switch(this.props.roomState) {
        case ROOM_STATES.INIT:
        case ROOM_STATES.READY: {
          return (
            React.DOM.button({className: "btn btn-join btn-info", 
                    onClick: this.props.joinRoom}, 
              mozL10n.get("rooms_room_join_label")
            )
          );
        }
        case ROOM_STATES.JOINED:
        case ROOM_STATES.SESSION_CONNECTED: {
          return (
            React.DOM.p({className: "empty-room-message"}, 
              mozL10n.get("rooms_only_occupant_label")
            )
          );
        }
        case ROOM_STATES.FULL:
          return (
            React.DOM.div(null, 
              React.DOM.p({className: "full-room-message"}, 
                mozL10n.get("rooms_room_full_label")
              ), 
              React.DOM.p(null, this._renderCallToActionLink())
            )
          );
        case ROOM_STATES.FAILED:
          return (
            React.DOM.p({className: "failed-room-message"}, 
              this._getFailureString()
            )
          );
        default:
          return null;
      }
    },

    render: function() {
      return (
        React.DOM.div({className: "room-inner-info-area"}, 
          this._renderContent()
        )
      );
    }
  });

  var StandaloneRoomHeader = React.createClass({displayName: 'StandaloneRoomHeader',
    render: function() {
      return (
        React.DOM.header(null, 
          React.DOM.h1(null, mozL10n.get("clientShortname2"))
        )
      );
    }
  });

  var StandaloneRoomFooter = React.createClass({displayName: 'StandaloneRoomFooter',
    _getContent: function() {
      return mozL10n.get("legal_text_and_links", {
        "clientShortname": mozL10n.get("clientShortname2"),
        "terms_of_use_url": React.renderComponentToStaticMarkup(
          React.DOM.a({href: loop.config.legalWebsiteUrl, target: "_blank"}, 
            mozL10n.get("terms_of_use_link_text")
          )
        ),
        "privacy_notice_url": React.renderComponentToStaticMarkup(
          React.DOM.a({href: loop.config.privacyWebsiteUrl, target: "_blank"}, 
            mozL10n.get("privacy_notice_link_text")
          )
        ),
      });
    },

    render: function() {
      return (
        React.DOM.footer(null, 
          React.DOM.p({dangerouslySetInnerHTML: {__html: this._getContent()}}), 
          React.DOM.div({className: "footer-logo"})
        )
      );
    }
  });

  var StandaloneRoomView = React.createClass({displayName: 'StandaloneRoomView',
    mixins: [Backbone.Events],

    propTypes: {
      activeRoomStore:
        React.PropTypes.instanceOf(loop.store.ActiveRoomStore).isRequired,
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

    /**
     * Returns either the required DOMNode
     *
     * @param {String} className The name of the class to get the element for.
     */
    _getElement: function(className) {
      return this.getDOMNode().querySelector(className);
    },

     /**
     * Returns the required configuration for publishing video on the sdk.
     */
    _getPublisherConfig: function() {
      // height set to 100%" to fix video layout on Google Chrome
      // @see https://bugzilla.mozilla.org/show_bug.cgi?id=1020445
      return {
        insertMode: "append",
        width: "100%",
        height: "100%",
        publishVideo: true,
        style: {
          audioLevelDisplayMode: "off",
          bugDisplayMode: "off",
          buttonDisplayMode: "off",
          nameDisplayMode: "off",
          videoDisabledDisplayMode: "off"
        }
      };
    },

    componentDidMount: function() {
      // Adding a class to the document body element from here to ease styling it.
      document.body.classList.add("is-standalone-room");
    },

    componentWillUnmount: function() {
      this.stopListening(this.props.activeRoomStore);
    },

    /**
     * Watches for when we transition from READY to JOINED room state, so we can
     * request user media access.
     * @param  {Object} nextProps (Unused)
     * @param  {Object} nextState Next state object.
     */
    componentWillUpdate: function(nextProps, nextState) {
      if (this.state.roomState === ROOM_STATES.READY &&
          nextState.roomState === ROOM_STATES.JOINED) {
        this.props.dispatcher.dispatch(new sharedActions.SetupStreamElements({
          publisherConfig: this._getPublisherConfig(),
          getLocalElementFunc: this._getElement.bind(this, ".local"),
          getRemoteElementFunc: this._getElement.bind(this, ".remote")
        }));
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
        "local-stream-audio": false
      });

      return (
        React.DOM.div({className: "room-conversation-wrapper"}, 
          StandaloneRoomHeader(null), 
          StandaloneRoomInfoArea({roomState: this.state.roomState, 
                                  failureReason: this.state.failureReason, 
                                  joinRoom: this.joinRoom, 
                                  helper: this.props.helper}), 
          React.DOM.div({className: "video-layout-wrapper"}, 
            React.DOM.div({className: "conversation room-conversation"}, 
              React.DOM.h2({className: "room-name"}, this.state.roomName), 
              React.DOM.div({className: "media nested"}, 
                React.DOM.div({className: "video_wrapper remote_wrapper"}, 
                  React.DOM.div({className: "video_inner remote"})
                ), 
                React.DOM.div({className: localStreamClasses})
              ), 
              sharedViews.ConversationToolbar({
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
          StandaloneRoomFooter(null)
        )
      );
    }
  });

  return {
    StandaloneRoomView: StandaloneRoomView
  };
})(navigator.mozL10n);
