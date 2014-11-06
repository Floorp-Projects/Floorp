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
   * - {loop.Dispatcher} dispatcher       The dispatcher for dispatching actions
   *                                      and registering to consume actions.
   * - {mozLoop}         mozLoop          The MozLoop API object.
   * - {ActiveRoomStore} activeRoomStore  An optional substore for active room
   *                                      state.
   *
   * @extends {Backbone.Events}
   * @param {Object} options Options object.
   */
  function RoomStore(options) {
    options = options || {};

    if (!options.dispatcher) {
      throw new Error("Missing option dispatcher");
    }
    this._dispatcher = options.dispatcher;

    if (!options.mozLoop) {
      throw new Error("Missing option mozLoop");
    }
    this._mozLoop = options.mozLoop;

    if (options.activeRoomStore) {
      this.activeRoomStore = options.activeRoomStore;
      this.activeRoomStore.on("change",
                              this._onActiveRoomStoreChange.bind(this));
    }

    this._dispatcher.register(this, [
      "createRoom",
      "createRoomError",
      "copyRoomUrl",
      "deleteRoom",
      "deleteRoomError",
      "getAllRooms",
      "getAllRoomsError",
      "openRoom",
      "updateRoomList"
    ]);
  }

  RoomStore.prototype = _.extend({
    /**
     * Maximum size given to createRoom; only 2 is supported (and is
     * always passed) because that's what the user-experience is currently
     * designed and tested to handle.
     * @type {Number}
     */
    maxRoomCreationSize: 2,

    /**
     * The number of hours for which the room will exist - default 8 weeks
     * @type {Number}
     */
    defaultExpiresIn: 24 * 7 * 8,

    /**
     * Internal store state representation.
     * @type {Object}
     * @see  #getStoreState
     */
    _storeState: {
      activeRoom: {},
      error: null,
      pendingCreation: false,
      pendingInitialRetrieval: false,
      rooms: []
    },

    /**
     * Retrieves current store state. The returned state object holds the
     * following properties:
     *
     * - {Boolean} pendingCreation         Pending room creation flag.
     * - {Boolean} pendingInitialRetrieval Pending initial list retrieval flag.
     * - {Array}   rooms                   The current room list.
     * - {Error}   error                   Latest error encountered, if any.
     * - {Object}  activeRoom              Active room data, if any.
     *
     * You can request a given state property by providing the `key` argument.
     *
     * @param  {String|undefined} key An optional state property name.
     * @return {Object}
     */
    getStoreState: function(key) {
      if (key) {
        return this._storeState[key];
      }
      return this._storeState;
    },

    /**
     * Updates store state and trigger a global "change" event, plus one for
     * each provided newState property:
     *
     * - change:rooms
     * - change:pendingInitialRetrieval
     * - change:pendingCreation
     * - change:error
     * - change:activeRoom
     *
     * @param {Object} newState The new store state object.
     */
    setStoreState: function(newState) {
      for (var key in newState) {
        this._storeState[key] = newState[key];
        this.trigger("change:" + key);
      }
      this.trigger("change");
    },

    /**
     * Registers mozLoop.rooms events.
     */
    startListeningToRoomEvents: function() {
      // Rooms event registration
      this._mozLoop.rooms.on("add", this._onRoomAdded.bind(this));
      this._mozLoop.rooms.on("update", this._onRoomUpdated.bind(this));
      this._mozLoop.rooms.on("delete", this._onRoomRemoved.bind(this));
    },

    /**
     * Updates active room store state.
     */
    _onActiveRoomStoreChange: function() {
      this.setStoreState({activeRoom: this.activeRoomStore.getStoreState()});
    },

    /**
     * Local proxy helper to dispatch an action.
     *
     * @param {Action} action The action to dispatch.
     */
    _dispatchAction: function(action) {
      this._dispatcher.dispatch(action);
    },

    /**
     * Updates current room list when a new room is available.
     *
     * @param {String} eventName     The event name (unused).
     * @param {Object} addedRoomData The added room data.
     */
    _onRoomAdded: function(eventName, addedRoomData) {
      addedRoomData.participants = [];
      addedRoomData.ctime = new Date().getTime();
      this._dispatchAction(new sharedActions.UpdateRoomList({
        roomList: this._storeState.rooms.concat(new Room(addedRoomData))
      }));
    },

    /**
     * Executed when a room is updated.
     *
     * @param {String} eventName       The event name (unused).
     * @param {Object} updatedRoomData The updated room data.
     */
    _onRoomUpdated: function(eventName, updatedRoomData) {
      this._dispatchAction(new sharedActions.UpdateRoomList({
        roomList: this._storeState.rooms.map(function(room) {
          return room.roomToken === updatedRoomData.roomToken ?
                 updatedRoomData : room;
        })
      }));
    },

    /**
     * Executed when a room is deleted.
     *
     * @param {String} eventName       The event name (unused).
     * @param {Object} removedRoomData The removed room data.
     */
    _onRoomRemoved: function(eventName, removedRoomData) {
      this._dispatchAction(new sharedActions.UpdateRoomList({
        roomList: this._storeState.rooms.filter(function(room) {
          return room.roomToken !== removedRoomData.roomToken;
        })
      }));
    },

    /**
     * Maps and sorts the raw room list received from the mozLoop API.
     *
     * @param  {Array} rawRoomList Raw room list.
     * @return {Array}
     */
    _processRoomList: function(rawRoomList) {
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
     * Finds the next available room number in the provided room list.
     *
     * @param  {String} nameTemplate The room name template; should contain a
     *                               {{conversationLabel}} placeholder.
     * @return {Number}
     */
    findNextAvailableRoomNumber: function(nameTemplate) {
      var searchTemplate = nameTemplate.replace("{{conversationLabel}}", "");
      var searchRegExp = new RegExp("^" + searchTemplate + "(\\d+)$");

      var roomNumbers = this._storeState.rooms.map(function(room) {
        var match = searchRegExp.exec(room.roomName);
        return match && match[1] ? parseInt(match[1], 10) : 0;
      });

      if (!roomNumbers.length) {
        return 1;
      }

      return Math.max.apply(null, roomNumbers) + 1;
    },

    /**
     * Generates a room names against the passed template string.
     *
     * @param  {String} nameTemplate The room name template.
     * @return {String}
     */
    _generateNewRoomName: function(nameTemplate) {
      var roomLabel = this.findNextAvailableRoomNumber(nameTemplate);
      return nameTemplate.replace("{{conversationLabel}}", roomLabel);
    },

    /**
     * Creates a new room.
     *
     * @param {sharedActions.CreateRoom} actionData The new room information.
     */
    createRoom: function(actionData) {
      this.setStoreState({pendingCreation: true});

      var roomCreationData = {
        roomName:  this._generateNewRoomName(actionData.nameTemplate),
        roomOwner: actionData.roomOwner,
        maxSize:   this.maxRoomCreationSize,
        expiresIn: this.defaultExpiresIn
      };

      this._mozLoop.rooms.create(roomCreationData, function(err) {
        this.setStoreState({pendingCreation: false});
        if (err) {
          this._dispatchAction(new sharedActions.CreateRoomError({error: err}));
        }
      }.bind(this));
    },

    /**
     * Executed when a room creation error occurs.
     *
     * @param {sharedActions.CreateRoomError} actionData The action data.
     */
    createRoomError: function(actionData) {
      this.setStoreState({
        error: actionData.error,
        pendingCreation: false
      });
    },

    /**
     * Copy a room url.
     *
     * @param  {sharedActions.CopyRoomUrl} actionData The action data.
     */
    copyRoomUrl: function(actionData) {
      this._mozLoop.copyString(actionData.roomUrl);
    },

    /**
     * Creates a new room.
     *
     * @param {sharedActions.DeleteRoom} actionData The action data.
     */
    deleteRoom: function(actionData) {
      this._mozLoop.rooms.delete(actionData.roomToken, function(err) {
        if (err) {
         this._dispatchAction(new sharedActions.DeleteRoomError({error: err}));
        }
      }.bind(this));
    },

    /**
     * Executed when a room deletion error occurs.
     *
     * @param {sharedActions.DeleteRoomError} actionData The action data.
     */
    deleteRoomError: function(actionData) {
      this.setStoreState({error: actionData.error});
    },

    /**
     * Gather the list of all available rooms from the MozLoop API.
     */
    getAllRooms: function() {
      this.setStoreState({pendingInitialRetrieval: true});
      this._mozLoop.rooms.getAll(null, function(err, rawRoomList) {
        var action;

        this.setStoreState({pendingInitialRetrieval: false});

        if (err) {
          action = new sharedActions.GetAllRoomsError({error: err});
        } else {
          action = new sharedActions.UpdateRoomList({roomList: rawRoomList});
        }

        this._dispatchAction(action);

        // We can only start listening to room events after getAll() has been
        // called executed first.
        this.startListeningToRoomEvents();
      }.bind(this));
    },

    /**
     * Updates current error state in case getAllRooms failed.
     *
     * @param {sharedActions.GetAllRoomsError} actionData The action data.
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
        rooms: this._processRoomList(actionData.roomList)
      });
    },

    /**
     * Opens a room
     *
     * @param {sharedActions.OpenRoom} actionData The action data.
     */
    openRoom: function(actionData) {
      this._mozLoop.rooms.open(actionData.roomToken);
    }
  }, Backbone.Events);

  loop.store.RoomStore = RoomStore;
})();
