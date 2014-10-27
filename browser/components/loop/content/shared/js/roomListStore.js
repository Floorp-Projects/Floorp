/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.store = loop.store || {};

(function() {
  "use strict";

  /**
   * Shared actions.
   * @type {Object}
   */
  var sharedActions = loop.shared.actions;

  /**
   * Room validation schema. See validate.js.
   * @type {Object}
   */
  var roomSchema = {
    roomToken:    String,
    roomUrl:      String,
    roomName:     String,
    maxSize:      Number,
    participants: Array,
    ctime:        Number
  };

  /**
   * Room type. Basically acts as a typed object constructor.
   *
   * @param {Object} values Room property values.
   */
  function Room(values) {
    var validatedData = new loop.validate.Validator(roomSchema || {})
                                         .validate(values || {});
    for (var prop in validatedData) {
      this[prop] = validatedData[prop];
    }
  }

  loop.store.Room = Room;

  /**
   * Room store.
   *
   * Options:
   * - {loop.Dispatcher} dispatcher The dispatcher for dispatching actions and
   *                                registering to consume actions.
   * - {mozLoop}         mozLoop    The MozLoop API object.
   *
   * @extends {Backbone.Events}
   * @param {Object} options Options object.
   */
  function RoomListStore(options) {
    options = options || {};
    this.storeState = {error: null, rooms: []};

    if (!options.dispatcher) {
      throw new Error("Missing option dispatcher");
    }
    this.dispatcher = options.dispatcher;

    if (!options.mozLoop) {
      throw new Error("Missing option mozLoop");
    }
    this.mozLoop = options.mozLoop;

    this.dispatcher.register(this, [
      "getAllRooms",
      "getAllRoomsError",
      "openRoom",
      "updateRoomList"
    ]);
  }

  RoomListStore.prototype = _.extend({
    /**
     * Retrieves current store state.
     *
     * @return {Object}
     */
    getStoreState: function() {
      return this.storeState;
    },

    /**
     * Updates store states and trigger a "change" event.
     *
     * @param {Object} state The new store state.
     */
    setStoreState: function(state) {
      this.storeState = state;
      this.trigger("change");
    },

    /**
     * Maps and sorts the raw room list received from the mozLoop API.
     *
     * @param  {Array} rawRoomList Raw room list.
     * @return {Array}
     */
    _processRawRoomList: function(rawRoomList) {
      if (!rawRoomList) {
        return [];
      }
      return rawRoomList
        .map(function(rawRoom) {
          return new Room(rawRoom);
        })
        .slice()
        .sort(function(a, b) {
          return b.ctime - a.ctime;
        });
    },

    /**
     * Gather the list of all available rooms from the MozLoop API.
     */
    getAllRooms: function() {
      this.mozLoop.rooms.getAll(function(err, rawRoomList) {
        var action;
        if (err) {
          action = new sharedActions.GetAllRoomsError({error: err});
        } else {
          action = new sharedActions.UpdateRoomList({roomList: rawRoomList});
        }
        this.dispatcher.dispatch(action);
      }.bind(this));
    },

    /**
     * Updates current error state in case getAllRooms failed.
     *
     * @param {sharedActions.UpdateRoomListError} actionData The action data.
     */
    getAllRoomsError: function(actionData) {
      this.setStoreState({error: actionData.error});
    },

    /**
     * Updates current room list.
     *
     * @param {sharedActions.UpdateRoomList} actionData The action data.
     */
    updateRoomList: function(actionData) {
      this.setStoreState({
        error: undefined,
        rooms: this._processRawRoomList(actionData.roomList)
      });
    },
  }, Backbone.Events);

  loop.store.RoomListStore = RoomListStore;
})();
