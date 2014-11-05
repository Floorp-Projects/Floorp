/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true, React */

var loop = loop || {};
loop.roomViews = (function(mozL10n) {
  "use strict";

  var DesktopRoomView = React.createClass({
    mixins: [Backbone.Events, loop.shared.mixins.DocumentTitleMixin],

    propTypes: {
      mozLoop:
        React.PropTypes.object.isRequired,
      localRoomStore:
        React.PropTypes.instanceOf(loop.store.LocalRoomStore).isRequired,
    },

    getInitialState: function() {
      return this.props.localRoomStore.getStoreState();
    },

    componentWillMount: function() {
      this.listenTo(this.props.localRoomStore, "change",
        this._onLocalRoomStoreChanged);
    },

    /**
     * Handles a "change" event on the localRoomStore, and updates this.state
     * to match the store.
     *
     * @private
     */
    _onLocalRoomStoreChanged: function() {
      this.setState(this.props.localRoomStore.getStoreState());
    },

    componentWillUnmount: function() {
      this.stopListening(this.props.localRoomStore);
    },

    render: function() {
      if (this.state.serverData && this.state.serverData.roomName) {
        this.setTitle(this.state.serverData.roomName);
      }

      return (
        <div className="goat"/>
      );
    }
  });

  return {
    DesktopRoomView: DesktopRoomView
  };

})(document.mozL10n || navigator.mozL10n);;
