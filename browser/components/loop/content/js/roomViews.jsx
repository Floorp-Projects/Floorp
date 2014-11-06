/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true, React */

var loop = loop || {};
loop.roomViews = (function(mozL10n) {
  "use strict";

  var ROOM_STATES = loop.store.ROOM_STATES;

  var DesktopRoomView = React.createClass({
    mixins: [Backbone.Events, loop.shared.mixins.DocumentTitleMixin],

    propTypes: {
      mozLoop:   React.PropTypes.object.isRequired,
      roomStore: React.PropTypes.instanceOf(loop.store.RoomStore).isRequired,
    },

    getInitialState: function() {
      return this.props.roomStore.getStoreState("activeRoom");
    },

    componentWillMount: function() {
      this.listenTo(this.props.roomStore, "change:activeRoom",
                    this._onActiveRoomStateChanged);
    },

    /**
     * Handles a "change" event on the roomStore, and updates this.state
     * to match the store.
     *
     * @private
     */
    _onActiveRoomStateChanged: function() {
      this.setState(this.props.roomStore.getStoreState("activeRoom"));
    },

    componentWillUnmount: function() {
      this.stopListening(this.props.roomStore);
    },

    /**
     * Closes the window if the cancel button is pressed in the generic failure view.
     */
    closeWindow: function() {
      window.close();
    },

    render: function() {
      if (this.state.roomName) {
        this.setTitle(this.state.roomName);
      }

      if (this.state.roomState === ROOM_STATES.FAILED) {
        return (<loop.conversation.GenericFailureView
          cancelCall={this.closeWindow}
        />);
      }

      return (
        <div>
          <div>{mozL10n.get("invite_header_text")}</div>
        </div>
      );
    }
  });

  return {
    DesktopRoomView: DesktopRoomView
  };

})(document.mozL10n || navigator.mozL10n);
