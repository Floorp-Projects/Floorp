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
      return this.props.roomStore.getStoreState("activeRoom");
    }
  };

  /**
   * Desktop room invitation view (overlay).
   */
  var DesktopRoomInvitationView = React.createClass({
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
        <div className="room-conversation-wrapper">
          <div className="room-invitation-overlay">
            <form onSubmit={this.handleFormSubmit}>
              <input type="text" ref="roomName"
                placeholder={mozL10n.get("rooms_name_this_room_label")} />
            </form>
            <p>{mozL10n.get("invite_header_text")}</p>
            <div className="btn-group call-action-group">
              <button className="btn btn-info btn-email"
                      onClick={this.handleEmailButtonClick}>
                {mozL10n.get("share_button2")}
              </button>
              <button className="btn btn-info btn-copy"
                      onClick={this.handleCopyButtonClick}>
                {mozL10n.get("copy_url_button2")}
              </button>
            </div>
          </div>
          <DesktopRoomConversationView roomStore={this.props.roomStore} />
        </div>
      );
    }
  });

  /**
   * Desktop room conversation view.
   */
  var DesktopRoomConversationView = React.createClass({
    mixins: [ActiveRoomStoreMixin],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
      video: React.PropTypes.object,
      audio: React.PropTypes.object,
      displayInvitation: React.PropTypes.bool
    },

    getDefaultProps: function() {
      return {
        video: {enabled: true, visible: true},
        audio: {enabled: true, visible: true}
      };
    },

    render: function() {
      var localStreamClasses = React.addons.classSet({
        local: true,
        "local-stream": true,
        "local-stream-audio": !this.props.video.enabled
      });
      return (
        <div className="room-conversation-wrapper">
          <div className="video-layout-wrapper">
            <div className="conversation room-conversation">
              <div className="media nested">
                <div className="video_wrapper remote_wrapper">
                  <div className="video_inner remote"></div>
                </div>
                <div className={localStreamClasses}></div>
              </div>
              <sharedViews.ConversationToolbar
                video={this.props.video}
                audio={this.props.audio}
                publishStream={noop}
                hangup={noop} />
            </div>
          </div>
        </div>
      );
    }
  });

  /**
   * Desktop room controller view.
   */
  var DesktopRoomControllerView = React.createClass({
    mixins: [ActiveRoomStoreMixin, loop.shared.mixins.DocumentTitleMixin],

    propTypes: {
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired
    },

    closeWindow: function() {
      window.close();
    },

    _renderRoomView: function(roomState) {
      switch (roomState) {
        case ROOM_STATES.FAILED: {
          return <loop.conversation.GenericFailureView
            cancelCall={this.closeWindow}
          />;
        }
        case ROOM_STATES.INIT:
        case ROOM_STATES.GATHER:
        case ROOM_STATES.READY:
        case ROOM_STATES.JOINED: {
          return <DesktopRoomInvitationView
            dispatcher={this.props.dispatcher}
            roomStore={this.props.roomStore}
          />;
        }
        // XXX needs bug 1074686/1074702
        case ROOM_STATES.HAS_PARTICIPANTS: {
          return <DesktopRoomConversationView
            dispatcher={this.props.dispatcher}
            roomStore={this.props.roomStore}
          />;
        }
      }
    },

    render: function() {
      if (this.state.roomName) {
        this.setTitle(this.state.roomName);
      }
      return (
        <div className="room-conversation-wrapper">{
          this._renderRoomView(this.state.roomState)
        }</div>
      );
    }
  });

  return {
    ActiveRoomStoreMixin: ActiveRoomStoreMixin,
    DesktopRoomControllerView: DesktopRoomControllerView,
    DesktopRoomConversationView: DesktopRoomConversationView,
    DesktopRoomInvitationView: DesktopRoomInvitationView
  };

})(document.mozL10n || navigator.mozL10n);
