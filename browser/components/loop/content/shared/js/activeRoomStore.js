/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.store = loop.store || {};
loop.store.ActiveRoomStore = (function() {
  "use strict";

  var sharedActions = loop.shared.actions;

  var ROOM_STATES = loop.store.ROOM_STATES = {
    // The initial state of the room
    INIT: "room-init",
    // The store is gathering the room data
    GATHER: "room-gather",
    // The store has got the room data
    READY: "room-ready",
    // The room is known to be joined on the loop-server
    JOINED: "room-joined",
    // There was an issue with the room
    FAILED: "room-failed",
    // XXX to be implemented in bug 1074686/1074702
    HAS_PARTICIPANTS: "room-has-participants"
  };

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
  function ActiveRoomStore(options) {
    options = options || {};

    if (!options.dispatcher) {
      throw new Error("Missing option dispatcher");
    }
    this._dispatcher = options.dispatcher;

    if (!options.mozLoop) {
      throw new Error("Missing option mozLoop");
    }
    this._mozLoop = options.mozLoop;

    this._dispatcher.register(this, [
      "roomFailure",
      "setupWindowData",
      "fetchServerData",
      "updateRoomInfo",
      "joinRoom",
      "joinedRoom",
      "windowUnload",
      "leaveRoom"
    ]);

    /**
     * Stored data reflecting the local state of a given room, used to drive
     * the room's views.
     *
     * @see https://wiki.mozilla.org/Loop/Architecture/Rooms#GET_.2Frooms.2F.7Btoken.7D
     *      for the main data. Additional properties below.
     *
     * @property {ROOM_STATES} roomState - the state of the room.
     * @property {Error=} error - if the room is an error state, this will be
     *                            set to an Error object reflecting the problem;
     *                            otherwise it will be unset.
     */
    this._storeState = {
      roomState: ROOM_STATES.INIT
    };
  }

  ActiveRoomStore.prototype = _.extend({
    /**
     * The time factor to adjust the expires time to ensure that we send a refresh
     * before the expiry. Currently set as 90%.
     */
    expiresTimeFactor: 0.9,

    getStoreState: function() {
      return this._storeState;
    },

    setStoreState: function(newState) {
      for (var key in newState) {
        this._storeState[key] = newState[key];
      }
      this.trigger("change");
    },

    /**
     * Handles a room failure. Currently this prints the error to the console
     * and sets the roomState to failed.
     *
     * @param {sharedActions.RoomFailure} actionData
     */
    roomFailure: function(actionData) {
      console.error("Error in state `" + this._storeState.roomState + "`:",
        actionData.error);

      this.setStoreState({
        error: actionData.error,
        roomState: ROOM_STATES.FAILED
      });
    },

    /**
     * Execute setupWindowData event action from the dispatcher. This gets
     * the room data from the mozLoop api, and dispatches an UpdateRoomInfo event.
     * It also dispatches JoinRoom as this action is only applicable to the desktop
     * client, and needs to auto-join.
     *
     * @param {sharedActions.SetupWindowData} actionData
     */
    setupWindowData: function(actionData) {
      if (actionData.type !== "room") {
        // Nothing for us to do here, leave it to other stores.
        return;
      }

      this.setStoreState({
        roomState: ROOM_STATES.GATHER
      });

      // Get the window data from the mozLoop api.
      this._mozLoop.rooms.get(actionData.roomToken,
        function(error, roomData) {
          if (error) {
            this._dispatcher.dispatch(new sharedActions.RoomFailure({
              error: error
            }));
            return;
          }

          this._dispatcher.dispatch(
            new sharedActions.UpdateRoomInfo({
            roomToken: actionData.roomToken,
            roomName: roomData.roomName,
            roomOwner: roomData.roomOwner,
            roomUrl: roomData.roomUrl
          }));

          // For the conversation window, we need to automatically
          // join the room.
          this._dispatcher.dispatch(new sharedActions.JoinRoom());
        }.bind(this));
    },

    /**
     * Execute fetchServerData event action from the dispatcher. Although
     * this is to fetch the server data - for rooms on the standalone client,
     * we don't actually need to get any data. Therefore we just save the
     * data that is given to us for when the user chooses to join the room.
     *
     * @param {sharedActions.FetchServerData} actionData
     */
    fetchServerData: function(actionData) {
      if (actionData.windowType !== "room") {
        // Nothing for us to do here, leave it to other stores.
        return;
      }

      this.setStoreState({
        roomToken: actionData.token,
        roomState: ROOM_STATES.READY
      });
    },

    /**
     * Handles the updateRoomInfo action. Updates the room data and
     * sets the state to `READY`.
     *
     * @param {sharedActions.UpdateRoomInfo} actionData
     */
    updateRoomInfo: function(actionData) {
      this.setStoreState({
        roomName: actionData.roomName,
        roomOwner: actionData.roomOwner,
        roomState: ROOM_STATES.READY,
        roomToken: actionData.roomToken,
        roomUrl: actionData.roomUrl
      });
    },

    /**
     * Handles the action to join to a room.
     */
    joinRoom: function() {
      this._mozLoop.rooms.join(this._storeState.roomToken,
        function(error, responseData) {
          if (error) {
            this._dispatcher.dispatch(
              new sharedActions.RoomFailure({error: error}));
            return;
          }

          this._dispatcher.dispatch(new sharedActions.JoinedRoom({
            apiKey: responseData.apiKey,
            sessionToken: responseData.sessionToken,
            sessionId: responseData.sessionId,
            expires: responseData.expires
          }));
        }.bind(this));
    },

    /**
     * Handles the data received from joining a room. It stores the relevant
     * data, and sets up the refresh timeout for ensuring membership of the room
     * is refreshed regularly.
     *
     * @param {sharedActions.JoinedRoom} actionData
     */
    joinedRoom: function(actionData) {
      this.setStoreState({
        apiKey: actionData.apiKey,
        sessionToken: actionData.sessionToken,
        sessionId: actionData.sessionId,
        roomState: ROOM_STATES.JOINED
      });

      this._setRefreshTimeout(actionData.expires);
    },

    /**
     * Handles the window being unloaded. Ensures the room is left.
     */
    windowUnload: function() {
      this._leaveRoom();
    },

    /**
     * Handles a room being left.
     */
    leaveRoom: function() {
      this._leaveRoom();
    },

    /**
     * Handles setting of the refresh timeout callback.
     *
     * @param {Integer} expireTime The time until expiry (in seconds).
     */
    _setRefreshTimeout: function(expireTime) {
      this._timeout = setTimeout(this._refreshMembership.bind(this),
        expireTime * this.expiresTimeFactor * 1000);
    },

    /**
     * Refreshes the membership of the room with the server, and then
     * sets up the refresh for the next cycle.
     */
    _refreshMembership: function() {
      this._mozLoop.rooms.refreshMembership(this._storeState.roomToken,
        this._storeState.sessionToken,
        function(error, responseData) {
          if (error) {
            this._dispatcher.dispatch(
              new sharedActions.RoomFailure({error: error}));
            return;
          }

          this._setRefreshTimeout(responseData.expires);
        }.bind(this));
    },

    /**
     * Handles leaving a room. Clears any membership timeouts, then
     * signals to the server the leave of the room.
     */
    _leaveRoom: function() {
      if (this._storeState.roomState !== ROOM_STATES.JOINED) {
        return;
      }

      if (this._timeout) {
        clearTimeout(this._timeout);
        delete this._timeout;
      }

      this._mozLoop.rooms.leave(this._storeState.roomToken,
        this._storeState.sessionToken);

      this.setStoreState({
        roomState: ROOM_STATES.READY
      });
    }

  }, Backbone.Events);

  return ActiveRoomStore;

})();
