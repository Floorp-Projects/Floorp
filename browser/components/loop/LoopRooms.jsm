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

// Create a new instance of the ConsoleAPI so we can control the maxLogLevel with a pref.
XPCOMUtils.defineLazyGetter(this, "log", () => {
  let ConsoleAPI = Cu.import("resource://gre/modules/devtools/Console.jsm", {}).ConsoleAPI;
  let consoleOptions = {
    maxLogLevel: Services.prefs.getCharPref(PREF_LOG_LEVEL).toLowerCase(),
    prefix: "Loop",
  };
  return new ConsoleAPI(consoleOptions);
});

this.EXPORTED_SYMBOLS = ["LoopRooms", "roomsPushNotification"];

let gRoomsListFetched = false;
let gRooms = new Map();

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
        room.localRoomID = id;
        // Next, request the detailed information for each room.
        // If the request fails the room data will not be added to the map.
        try {
          let details = yield this.requestRoomDetails(room.roomToken, sessionType);
          for (let attr in details) {
            room[attr] = details[attr]
          }
          gRooms.set(id, room);
        }
        catch (error) {log.warn("failed GETing room details for roomToken = " + room.roomToken + ": ", error)}
      }
      callback(null, [...gRooms.values()]);
      return;
      }.bind(this)).catch((error) => {log.error("getAll error:", error);
                                      callback(error)});
    return;
  },

  getRoomData: function(roomID, callback) {
    if (gRooms.has(roomID)) {
      callback(null, gRooms.get(roomID));
    } else {
      callback(new Error("Room data not found or not fetched yet for room with ID " + roomID));
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
   * @param {String} roomID Local room identifier
   * @param {Function} callback Function that will be invoked once the operation
   *                            finished. The first argument passed will be an
   *                            `Error` object or `null`. The second argument will
   *                            be the list of rooms, if it was fetched successfully.
   */
  getRoomData: function(roomID, callback) {
    return LoopRoomsInternal.getRoomData(roomID, callback);
  },
};
Object.freeze(LoopRooms);

