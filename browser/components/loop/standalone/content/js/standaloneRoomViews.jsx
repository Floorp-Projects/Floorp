/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true, React */

var loop = loop || {};
loop.standaloneRoomViews = (function(mozL10n) {
  "use strict";

  var ROOM_STATES = loop.store.ROOM_STATES;
  var sharedActions = loop.shared.actions;
  var sharedViews = loop.shared.views;

  var StandaloneRoomView = React.createClass({
    mixins: [Backbone.Events],

    propTypes: {
      activeRoomStore:
        React.PropTypes.instanceOf(loop.store.ActiveRoomStore).isRequired,
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
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

    _renderContextualRoomInfo: function() {
      switch(this.state.roomState) {
        case ROOM_STATES.INIT:
        case ROOM_STATES.READY: {
          // Join button
          return (
            <div className="room-inner-info-area">
              <button className="btn btn-join btn-info" onClick={this.joinRoom}>
                {mozL10n.get("rooms_room_join_label")}
              </button>
            </div>
          );
        }
        case ROOM_STATES.JOINED:
        case ROOM_STATES.SESSION_CONNECTED: {
          // Empty room message
          return (
            <div className="room-inner-info-area">
              <p className="empty-room-message">
                {mozL10n.get("rooms_only_occupant_label")}
              </p>
            </div>
          );
        }
      }
      // XXX Render "Start your own" button when room is over capacity (see
      //     bug 1074709)
    },

    render: function() {
      var localStreamClasses = React.addons.classSet({
        hide: !this._roomIsActive(),
        local: true,
        "local-stream": true,
        "local-stream-audio": false
      });

      return (
        <div className="room-conversation-wrapper">
          {this._renderContextualRoomInfo()}
          <div className="video-layout-wrapper">
            <div className="conversation room-conversation">
              <h2 className="room-name">{this.state.roomName}</h2>
              <div className="media nested">
                <div className="video_wrapper remote_wrapper">
                  <div className="video_inner remote"></div>
                </div>
                <div className={localStreamClasses}></div>
              </div>
              <sharedViews.ConversationToolbar
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
        </div>
      );
    }
  });

  return {
    StandaloneRoomView: StandaloneRoomView
  };
})(navigator.mozL10n);
