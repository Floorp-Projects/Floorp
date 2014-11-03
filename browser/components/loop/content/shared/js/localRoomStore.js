/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.store = loop.store || {};
loop.store.LocalRoomStore = (function() {
  "use strict";

  var sharedActions = loop.shared.actions;

  /**
   * Store for things that are local to this instance (in this profile, on
   * this machine) of this roomRoom store, in addition to a mirror of some
   * remote-state.
   *
   * @extends {Backbone.Events}
   *
   * @param {Object}          options - Options object
   * @param {loop.Dispatcher} options.dispatch - The dispatcher for dispatching
   *                            actions and registering to consume them.
   * @param {MozLoop}         options.mozLoop - MozLoop API provider object
   */
  function LocalRoomStore(options) {
    options = options || {};

    if (!options.dispatcher) {
      throw new Error("Missing option dispatcher");
    }
    this.dispatcher = options.dispatcher;

    if (!options.mozLoop) {
      throw new Error("Missing option mozLoop");
    }
    this.mozLoop = options.mozLoop;

    this.dispatcher.register(this, [
      "setupWindowData"
    ]);
  }

  LocalRoomStore.prototype = _.extend({

    /**
     * Stored data reflecting the local state of a given room, used to drive
     * the room's views.
     *
     * @property {Object} serverData - local cache of the data returned by
     *                                 MozLoop.getRoomData for this room.
     * @see https://wiki.mozilla.org/Loop/Architecture/Rooms#GET_.2Frooms.2F.7Btoken.7D
     *
     * @property {Error=} error - if the room is an error state, this will be
     *                            set to an Error object reflecting the problem;
     *                            otherwise it will be unset.
     *
     * @property {String} localRoomId - profile-local identifier used with
     *                                  the MozLoop API.
     */
    _storeState: {
    },

    getStoreState: function() {
      return this._storeState;
    },

    setStoreState: function(state) {
      this._storeState = state;
      this.trigger("change");
    },

    /**
     * Proxy to mozLoop.rooms.getRoomData for setupEmptyRoom action.
     *
     * XXXremoveMe Can probably be removed when bug 1074664 lands.
     *
     * @param {Integer} roomId The id of the room.
     * @param {Function} cb Callback(error, roomData)
     */
    _fetchRoomData: function(roomId, cb) {
      // XXX Remove me in bug 1074678
      if (!this.mozLoop.getLoopBoolPref("test.alwaysUseRooms")) {
        this.mozLoop.rooms.getRoomData(roomId, cb);
      } else {
        cb(null, {roomName: "Donkeys"});
      }
    },

    /**
     * Execute setupEmptyRoom event action from the dispatcher.  This primes
     * the store with the localRoomId, and calls MozLoop.getRoomData on that
     * ID.  This will return either a reflection of state on the server, or,
     * if the createRoom call hasn't yet returned, it will have at least the
     * roomName as specified to the createRoom method.
     *
     * When the room name gets set, that will trigger the view to display
     * that name.
     *
     * @param {sharedActions.SetupWindowData} actionData
     */
    setupWindowData: function(actionData) {
      if (actionData.windowData.type !== "room") {
        // Nothing for us to do here, leave it to other stores.
        return;
      }

      this._fetchRoomData(actionData.windowData.localRoomId,
        function(error, roomData) {
          this.setStoreState({
            error: error,
            localRoomId: actionData.windowData.localRoomId,
            serverData: roomData
          });
        }.bind(this));
    }

  }, Backbone.Events);

  return LocalRoomStore;

})();
