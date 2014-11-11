/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* jshint newcap:false */
/* global loop:true, React */

var loop = loop || {};
loop.roomViews = (function(mozL10n) {
  "use strict";

  var ROOM_STATES = loop.store.ROOM_STATES;
  var sharedViews = loop.shared.views;

  function noop() {}

  /**
   * ActiveRoomStore mixin.
   * @type {Object}
   */
  var ActiveRoomStoreMixin = {
    mixins: [Backbone.Events],

    propTypes: {
      roomStore: React.PropTypes.instanceOf(loop.store.RoomStore).isRequired
    },

    componentWillMount: function() {
      this.listenTo(this.props.roomStore, "change:activeRoom",
                    this._onActiveRoomStateChanged);
    },

    componentWillUnmount: function() {
      this.stopListening(this.props.roomStore);
    },

    _onActiveRoomStateChanged: function() {
      this.setState(this.props.roomStore.getStoreState("activeRoom"));
    },

    getInitialState: function() {
      var storeState = this.props.roomStore.getStoreState("activeRoom");
      return _.extend(storeState, {
        // Used by the UI showcase.
        roomState: this.props.roomState || storeState.roomState
      });
    }
  };

  /**
   * Desktop room invitation view (overlay).
   */
  var DesktopRoomInvitationView = React.createClass({displayName: 'DesktopRoomInvitationView',
    mixins: [ActiveRoomStoreMixin],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired
    },

    handleFormSubmit: function(event) {
      event.preventDefault();
      // XXX
    },

    handleEmailButtonClick: function(event) {
      event.preventDefault();
      // XXX
    },

    handleCopyButtonClick: function(event) {
      event.preventDefault();
      // XXX
    },

    render: function() {
      return (
        React.DOM.div({className: "room-invitation-overlay"}, 
          React.DOM.form({onSubmit: this.handleFormSubmit}, 
            React.DOM.input({type: "text", ref: "roomName", 
              placeholder: mozL10n.get("rooms_name_this_room_label")})
          ), 
          React.DOM.p(null, mozL10n.get("invite_header_text")), 
          React.DOM.div({className: "btn-group call-action-group"}, 
            React.DOM.button({className: "btn btn-info btn-email", 
                    onClick: this.handleEmailButtonClick}, 
              mozL10n.get("share_button2")
            ), 
            React.DOM.button({className: "btn btn-info btn-copy", 
                    onClick: this.handleCopyButtonClick}, 
              mozL10n.get("copy_url_button2")
            )
          )
        )
      );
    }
  });

  /**
   * Desktop room conversation view.
   */
  var DesktopRoomConversationView = React.createClass({displayName: 'DesktopRoomConversationView',
    mixins: [ActiveRoomStoreMixin, loop.shared.mixins.DocumentTitleMixin],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      video: React.PropTypes.object,
      audio: React.PropTypes.object
    },

    getDefaultProps: function() {
      return {
        video: {enabled: true, visible: true},
        audio: {enabled: true, visible: true}
      };
    },

    _renderInvitationOverlay: function() {
      if (this.state.roomState !== ROOM_STATES.HAS_PARTICIPANTS) {
        return DesktopRoomInvitationView({
          roomStore: this.props.roomStore, 
          dispatcher: this.props.dispatcher}
        );
      }
      return null;
    },

    render: function() {
      if (this.state.roomName) {
        this.setTitle(this.state.roomName);
      }

      var localStreamClasses = React.addons.classSet({
        local: true,
        "local-stream": true,
        "local-stream-audio": !this.props.video.enabled
      });

      switch(this.state.roomState) {
        case ROOM_STATES.FAILED: {
          return loop.conversation.GenericFailureView({
            cancelCall: this.closeWindow}
          );
        }
        default: {
          return (
            React.DOM.div({className: "room-conversation-wrapper"}, 
              this._renderInvitationOverlay(), 
              React.DOM.div({className: "video-layout-wrapper"}, 
                React.DOM.div({className: "conversation room-conversation"}, 
                  React.DOM.div({className: "media nested"}, 
                    React.DOM.div({className: "video_wrapper remote_wrapper"}, 
                      React.DOM.div({className: "video_inner remote"})
                    ), 
                    React.DOM.div({className: localStreamClasses})
                  ), 
                  sharedViews.ConversationToolbar({
                    video: this.props.video, 
                    audio: this.props.audio, 
                    publishStream: noop, 
                    hangup: noop})
                )
              )
            )
          );
        }
      }
    }
  });

  return {
    ActiveRoomStoreMixin: ActiveRoomStoreMixin,
    DesktopRoomConversationView: DesktopRoomConversationView,
    DesktopRoomInvitationView: DesktopRoomInvitationView
  };

})(document.mozL10n || navigator.mozL10n);
