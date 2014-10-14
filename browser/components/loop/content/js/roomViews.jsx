/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true, React */

var loop = loop || {};
loop.roomViews = (function(mozL10n) {
  "use strict";

  /**
   * Root object, by default set to window.
   * @type {DOMWindow|Object}
   */
  var rootObject = window;

  /**
   * Sets a new root object. This is useful for testing native DOM events so we
   * can fake them.
   *
   * @param {Object}
   */
  function setRootObject(obj) {
    rootObject = obj;
  }

  var EmptyRoomView = React.createClass({
    mixins: [Backbone.Events],

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

    componentDidMount: function() {
      // XXXremoveMe (just the conditional itself) in patch 2 for bug 1074686,
      // once the addCallback stuff lands
      if (this.props.mozLoop.rooms && this.props.mozLoop.rooms.addCallback) {
        this.props.mozLoop.rooms.addCallback(
          this.state.localRoomId,
          "RoomCreationError", this.onCreationError);
      }
    },

    /**
     * Attached to the "RoomCreationError" with mozLoop.rooms.addCallback,
     * which is fired mozLoop.rooms.createRoom from the panel encounters an
     * error while attempting to create the room for this view.
     *
     * @param {Error} err - JS Error object with info about the problem
     */
    onCreationError: function(err) {
      // XXX put up a user friendly error instead of this
      rootObject.console.error("EmptyRoomView creation error: ", err);
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

      // XXXremoveMe (just the conditional itself) in patch 2 for bug 1074686,
      // once the addCallback stuff lands
      if (this.props.mozLoop.rooms && this.props.mozLoop.rooms.removeCallback) {
        this.props.mozLoop.rooms.removeCallback(
          this.state.localRoomId,
          "RoomCreationError", this.onCreationError);
      }
    },

    render: function() {
      // XXX switch this to use the document title mixin once bug 1081079 lands
      if (this.state.serverData && this.state.serverData.roomName) {
        rootObject.document.title = this.state.serverData.roomName;
      }

      return (
        <div className="goat"/>
      );
    }
  });

  return {
    setRootObject: setRootObject,
    EmptyRoomView: EmptyRoomView
  };

})(document.mozL10n || navigator.mozL10n);;
