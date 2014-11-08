/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true, React */

var loop = loop || {};
loop.standaloneRoomViews = (function() {
  "use strict";

  var ROOM_STATES = loop.store.ROOM_STATES;
  var sharedActions = loop.shared.actions;

  var StandaloneRoomView = React.createClass({displayName: 'StandaloneRoomView',
    mixins: [Backbone.Events],

    propTypes: {
      activeRoomStore:
        React.PropTypes.instanceOf(loop.store.ActiveRoomStore).isRequired,
      dispatcher: React.PropTypes.instanceOf(loop.Dispatcher).isRequired,
    },

    getInitialState: function() {
      return this.props.activeRoomStore.getStoreState();
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

    componentWillUnmount: function() {
      this.stopListening(this.props.activeRoomStore);
    },

    joinRoom: function() {
      this.props.dispatcher.dispatch(new sharedActions.JoinRoom());
    },

    leaveRoom: function() {
      this.props.dispatcher.dispatch(new sharedActions.LeaveRoom());
    },

    // XXX Implement tests for this view when we do the proper views
    // - bug 1074705 and others
    render: function() {
      switch(this.state.roomState) {
        case ROOM_STATES.READY: {
          return (
            React.DOM.div(null, React.DOM.button({onClick: this.joinRoom}, "Join"))
          );
        }
        case ROOM_STATES.JOINED: {
          return (
            React.DOM.div(null, React.DOM.button({onClick: this.leaveRoom}, "Leave"))
          );
        }
        default: {
          return (
            React.DOM.div(null, this.state.roomState)
          );
        }
      }
    }
  });

  return {
    StandaloneRoomView: StandaloneRoomView
  };
})();
