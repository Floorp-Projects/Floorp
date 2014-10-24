/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "MozLoopService",
                                  "resource:///modules/loop/MozLoopService.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LOOP_SESSION_TYPE",
                                  "resource:///modules/loop/MozLoopService.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MozLoopPushHandler",
                                  "resource:///modules/loop/MozLoopPushHandler.jsm");

this.EXPORTED_SYMBOLS = ["LoopRooms", "roomsPushNotification"];

let gRoomsListFetched = false;
let gRooms = new Map();
let gCallbacks = new Map();

  /**
   * Callback used to indicate changes to rooms data on the LoopServer.
   *
   * @param {Object} version Version number assigned to this change set.
   * @param {Object} channelID Notification channel identifier.
   *
   */
const roomsPushNotification = function(version, channelID) {
    return LoopRoomsInternal.onNotification(version, channelID);
  };

let LoopRoomsInternal = {
  getAll: function(callback) {
    Task.spawn(function*() {
      yield MozLoopService.register();

      if (gRoomsListFetched) {
        callback(null, [...gRooms.values()]);
        return;
      }
      // Fetch the rooms from the server.
      let sessionType = MozLoopService.userProfile ? LOOP_SESSION_TYPE.FXA :
                        LOOP_SESSION_TYPE.GUEST;
      let rooms = yield this.requestRoomList(sessionType);
      // Add each room to our in-memory Map using a locally unique
      // identifier.
      for (let room of rooms) {
        let id = MozLoopService.generateLocalID();
        room.localRoomId = id;
        // Next, request the detailed information for each room.
        // If the request fails the room data will not be added to the map.
        try {
          let details = yield this.requestRoomDetails(room.roomToken, sessionType);
          for (let attr in details) {
            room[attr] = details[attr]
          }
          delete room.currSize; //This attribute will be eliminated in the next revision.
          gRooms.set(id, room);
        }
        catch (error) {MozLoopService.log.warn(
          "failed GETing room details for roomToken = " + room.roomToken + ": ", error)}
      }
      callback(null, [...gRooms.values()]);
      return;
      }.bind(this)).catch((error) => {MozLoopService.log.error("getAll error:", error);
                                      callback(error)});
    return;
  },

  getRoomData: function(localRoomId, callback) {
    if (gRooms.has(localRoomId)) {
      callback(null, gRooms.get(localRoomId));
    } else {
      callback(new Error("Room data not found or not fetched yet for room with ID " + localRoomId));
    }
    return;
  },

  /**
   * Request list of all rooms associated with this account.
   *
   * @param {String} sessionType Indicates which hawkRequest endpoint to use.
   *
   * @returns {Promise} room list
   */
  requestRoomList: function(sessionType) {
    return MozLoopService.hawkRequest(sessionType, "/rooms", "GET")
      .then(response => {
        let roomsList = JSON.parse(response.body);
        if (!Array.isArray(roomsList)) {
          // Force a reject in the returned promise.
          // To be caught by the caller using the returned Promise.
          throw new Error("Missing array of rooms in response.");
        }
        return roomsList;
      });
  },

  /**
   * Request information about a specific room from the server.
   *
   * @param {Object} token Room identifier returned from the LoopServer.
   * @param {String} sessionType Indicates which hawkRequest endpoint to use.
   *
   * @returns {Promise} room details
   */
  requestRoomDetails: function(token, sessionType) {
    return MozLoopService.hawkRequest(sessionType, "/rooms/" + token, "GET")
      .then(response => JSON.parse(response.body));
  },

  /**
   * Callback used to indicate changes to rooms data on the LoopServer.
   *
   * @param {Object} version Version number assigned to this change set.
   * @param {Object} channelID Notification channel identifier.
   *
   */
  onNotification: function(version, channelID) {
    return;
  },

  createRoom: function(props, callback) {
    // Always create a basic room record and launch the window, attaching
    // the localRoomId. Later errors will be returned via the registered callback.
    let localRoomId = MozLoopService.generateLocalID((id) => {gRooms.has(id)})
    let room = {localRoomId : localRoomId};
    for (let prop in props) {
      room[prop] = props[prop]
    }

    gRooms.set(localRoomId, room);
    this.addCallback(localRoomId, "RoomCreated", callback);
    MozLoopService.openChatWindow(null, "", "about:loopconversation#room/" + localRoomId);

    if (!"roomName" in props ||
        !"expiresIn" in props ||
        !"roomOwner" in props ||
        !"maxSize" in props) {
      this.postCallback(localRoomId, "RoomCreated",
                        new Error("missing required room create property"));
      return localRoomId;
    }

    let sessionType = MozLoopService.userProfile ? LOOP_SESSION_TYPE.FXA :
                                                   LOOP_SESSION_TYPE.GUEST;

    MozLoopService.hawkRequest(sessionType, "/rooms", "POST", props).then(
      (response) => {
        let data = JSON.parse(response.body);
        for (let attr in data) {
          room[attr] = data[attr]
        }
        delete room.expiresIn; //Do not keep this value - it is a request to the server
        this.postCallback(localRoomId, "RoomCreated", null, room);
      },
      (error) => {
        this.postCallback(localRoomId, "RoomCreated", error);
      });

    return localRoomId;
  },

  /**
   * Send an update to the callbacks registered for a specific localRoomId
   * for a callback type.
   *
   * The result set is always saved. Then each
   * callback function that has been registered when this function is
   * called will be called with the result set. Any new callback that
   * is regsitered via addCallback will receive a copy of the last
   * saved result set when registered. This allows the posting operation
   * to complete before the callback is registered in an asynchronous
   * operation.
   *
   * Callbacsk must be of the form:
   *    function (error, success) {...}
   *
   * @param {String} localRoomId Local room identifier.
   * @param {String} callbackName callback type
   * @param {?Error} error result or null.
   * @param {?Object} success result if error argument is null.
   */
  postCallback: function(localRoomId, callbackName, error, success) {
    let roomCallbacks = gCallbacks.get(localRoomId);
    if (!roomCallbacks) {
      // No callbacks have been registered or results posted for this room.
      // Initialize a record for this room and callbackName, saving the
      // result set.
      gCallbacks.set(localRoomId, new Map([[
        callbackName,
        { callbackList: [], result: { error: error, success: success } }]]));
      return;
    }

    let namedCallback = roomCallbacks.get(callbackName);
    // A callback of this name has not been registered.
    if (!namedCallback) {
      roomCallbacks.set(
        callbackName,
        {callbackList: [], result: {error: error, success: success}});
      return;
    }

    // Record the latest result set.
    namedCallback.result = {error: error, success: success};

    // Call each registerd callback passing the new result posted.
    namedCallback.callbackList.forEach((callback) => {
      callback(error, success);
    });
  },

  addCallback: function(localRoomId, callbackName, callback) {
    let roomCallbacks = gCallbacks.get(localRoomId);
    if (!roomCallbacks) {
      // No callbacks have been registered or results posted for this room.
      // Initialize a record for this room and callbackName.
      gCallbacks.set(localRoomId, new Map([[
        callbackName,
        {callbackList: [callback]}]]));
      return;
    }

    let namedCallback = roomCallbacks.get(callbackName);
    // A callback of this name has not been registered.
    if (!namedCallback) {
      roomCallbacks.set(
        callbackName,
        {callbackList: [callback]});
      return;
    }

    // Add this callback if not already in the array
    if (namedCallback.callbackList.indexOf(callback) >= 0) {
      return;
    }
    namedCallback.callbackList.push(callback);

    // If a result has been posted for this callback
    // send it using this new callback function.
    let result = namedCallback.result;
    if (result) {
      callback(result.error, result.success);
    }
  },

  deleteCallback: function(localRoomId, callbackName, callback) {
    let roomCallbacks = gCallbacks.get(localRoomId);
    if (!roomCallbacks) {
      return;
    }

    let namedCallback = roomCallbacks.get(callbackName);
    if (!namedCallback) {
      return;
    }

    let i = namedCallback.callbackList.indexOf(callback);
    if (i >= 0) {
      namedCallback.callbackList.splice(i, 1);
    }

    return;
  },
};
Object.freeze(LoopRoomsInternal);

/**
 * The LoopRooms class.
 *
 * Each method that is a member of this class requires the last argument to be a
 * callback Function. MozLoopAPI will cause things to break if this invariant is
 * violated. You'll notice this as well in the documentation for each method.
 */
this.LoopRooms = {
  /**
   * Fetch a list of rooms that the currently registered user is a member of.
   *
   * @param {Function} callback Function that will be invoked once the operation
   *                            finished. The first argument passed will be an
   *                            `Error` object or `null`. The second argument will
   *                            be the list of rooms, if it was fetched successfully.
   */
  getAll: function(callback) {
    return LoopRoomsInternal.getAll(callback);
  },

  /**
   * Return the current stored version of the data for the indicated room.
   *
   * @param {String} localRoomId Local room identifier
   * @param {Function} callback Function that will be invoked once the operation
   *                            finished. The first argument passed will be an
   *                            `Error` object or `null`. The second argument will
   *                            be the list of rooms, if it was fetched successfully.
   */
  getRoomData: function(localRoomId, callback) {
    return LoopRoomsInternal.getRoomData(localRoomId, callback);
  },

  /**
   * Create a room. Will both open a chat window for the new room
   * and perform an exchange with the LoopServer to create the room.
   * for a callback type. Callback must be of the form:
   *    function (error, success) {...}
   *
   * @param {Object} room properties to be sent to the LoopServer
   * @param {Function} callback Must be of the form: function (error, success) {...}
   *
   * @returns {String} localRoomId assigned to this new room.
   */
  createRoom: function(roomProps, callback) {
    return LoopRoomsInternal.createRoom(roomProps, callback);
  },

  /**
   * Register a callback of a specified type with a localRoomId.
   *
   * @param {String} localRoomId Local room identifier.
   * @param {String} callbackName callback type
   * @param {Function} callback Must be of the form: function (error, success) {...}
   */
  addCallback: function(localRoomId, callbackName, callback) {
    return LoopRoomsInternal.addCallback(localRoomId, callbackName, callback);
  },

  /**
   * Un-register and delete a callback of a specified type for a localRoomId.
   *
   * @param {String} localRoomId Local room identifier.
   * @param {String} callbackName callback type
   * @param {Function} callback Previously passed to addCallback().
   */
  deleteCallback: function(localRoomId, callbackName, callback) {
    return LoopRoomsInternal.deleteCallback(localRoomId, callbackName, callback);
  },
};
Object.freeze(LoopRooms);
