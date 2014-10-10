/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.store = loop.store || {};

(function() {
  "use strict";

  /**
   * Room validation schema. See validate.js.
   * @type {Object}
   */
  var roomSchema = {
    roomToken: String,
    roomUrl:   String,
    roomName:  String,
    maxSize:   Number,
    currSize:  Number,
    ctime:     Number
  };

  /**
   * Temporary sample raw room list data.
   * XXX Should be removed when we plug the real mozLoop API for rooms.
   *     See bug 1074664.
   * @type {Array}
   */
  var temporaryRawRoomList = [{
    roomToken: "_nxD4V4FflQ",
    roomUrl: "http://sample/_nxD4V4FflQ",
    roomName: "First Room Name",
    maxSize: 2,
    currSize: 0,
    ctime: 1405517546
  }, {
    roomToken: "QzBbvGmIZWU",
    roomUrl: "http://sample/QzBbvGmIZWU",
    roomName: "Second Room Name",
    maxSize: 2,
    currSize: 0,
    ctime: 1405517418
  }, {
    roomToken: "3jKS_Els9IU",
    roomUrl: "http://sample/3jKS_Els9IU",
    roomName: "Third Room Name",
    maxSize: 3,
    clientMaxSize: 2,
    currSize: 1,
    ctime: 1405518241
  }];

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
      "openRoom"
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
     * Proxy to navigator.mozLoop.rooms.getAll.
     * XXX Could probably be removed when bug 1074664 lands.
     *
     * @param  {Function} cb Callback(error, roomList)
     */
    _fetchRoomList: function(cb) {
      // Faking this.mozLoop.rooms until it's available; bug 1074664.
      if (!this.mozLoop.hasOwnProperty("rooms")) {
        cb(null, temporaryRawRoomList);
        return;
      }
      this.mozLoop.rooms.getAll(cb);
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
      this._fetchRoomList(function(err, rawRoomList) {
        this.setStoreState({
          error: err,
          rooms: this._processRawRoomList(rawRoomList)
        });
      }.bind(this));
    }
  }, Backbone.Events);

  loop.store.RoomListStore = RoomListStore;
})();
