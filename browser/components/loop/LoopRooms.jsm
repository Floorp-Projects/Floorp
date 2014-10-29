/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
const {MozLoopService, LOOP_SESSION_TYPE} = Cu.import("resource:///modules/loop/MozLoopService.jsm", {});
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyGetter(this, "eventEmitter", function() {
  const {EventEmitter} = Cu.import("resource://gre/modules/devtools/event-emitter.js", {});
  return new EventEmitter();
});

this.EXPORTED_SYMBOLS = ["LoopRooms", "roomsPushNotification"];

const roomsPushNotification = function(version, channelID) {
  return LoopRoomsInternal.onNotification(version, channelID);
};

// Since the LoopRoomsInternal.rooms map as defined below is a local cache of
// room objects that are retrieved from the server, this is list may become out
// of date. The Push server may notify us of this event, which will set the global
// 'dirty' flag to TRUE.
let gDirty = true;

/**
 * Extend a `target` object with the properties defined in `source`.
 *
 * @param {Object} target The target object to receive properties defined in `source`
 * @param {Object} source The source object to copy properties from
 */
const extend = function(target, source) {
  for (let key of Object.getOwnPropertyNames(source)) {
    target[key] = source[key];
  }
  return target;
};

/**
 * The Rooms class.
 *
 * Each method that is a member of this class requires the last argument to be a
 * callback Function. MozLoopAPI will cause things to break if this invariant is
 * violated. You'll notice this as well in the documentation for each method.
 */
let LoopRoomsInternal = {
  rooms: new Map(),

  /**
   * Fetch a list of rooms that the currently registered user is a member of.
   *
   * @param {String}   [version] If set, we will fetch a list of changed rooms since
   *                             `version`. Optional.
   * @param {Function} callback  Function that will be invoked once the operation
   *                             finished. The first argument passed will be an
   *                             `Error` object or `null`. The second argument will
   *                             be the list of rooms, if it was fetched successfully.
   */
  getAll: function(version = null, callback) {
    if (!callback) {
      callback = version;
      version = null;
    }

    Task.spawn(function* () {
      yield MozLoopService.register();

      if (!gDirty) {
        callback(null, [...this.rooms.values()]);
        return;
      }

      // Fetch the rooms from the server.
      let sessionType = MozLoopService.userProfile ? LOOP_SESSION_TYPE.FXA :
                        LOOP_SESSION_TYPE.GUEST;
      let url = "/rooms" + (version ? "?version=" + encodeURIComponent(version) : "");
      let response = yield MozLoopService.hawkRequest(sessionType, url, "GET");
      let roomsList = JSON.parse(response.body);
      if (!Array.isArray(roomsList)) {
        throw new Error("Missing array of rooms in response.");
      }

      // Next, request the detailed information for each room. If the request
      // fails the room data will not be added to the map.
      for (let room of roomsList) {
        let eventName = this.rooms.has(room.roomToken) ? "update" : "add";
        this.rooms.set(room.roomToken, room);
        yield LoopRooms.promise("get", room.roomToken);
      }

      // Set the 'dirty' flag back to FALSE, since the list is as fresh as can be now.
      gDirty = false;
      callback(null, [...this.rooms.values()]);
    }.bind(this)).catch(error => {
      callback(error);
    });
  },

  /**
   * Request information about a specific room from the server. It will be
   * returned from the cache if it's already in it.
   *
   * @param {String}   roomToken Room identifier
   * @param {Function} callback  Function that will be invoked once the operation
   *                             finished. The first argument passed will be an
   *                             `Error` object or `null`. The second argument will
   *                             be the list of rooms, if it was fetched successfully.
   */
  get: function(roomToken, callback) {
    let room = this.rooms.has(roomToken) ? this.rooms.get(roomToken) : {};
    // Check if we need to make a request to the server to collect more room data.
    if (!room || gDirty || !("participants" in room)) {
      let sessionType = MozLoopService.userProfile ? LOOP_SESSION_TYPE.FXA :
                        LOOP_SESSION_TYPE.GUEST;
      MozLoopService.hawkRequest(sessionType, "/rooms/" + encodeURIComponent(roomToken), "GET")
        .then(response => {
          let eventName = ("roomToken" in room) ? "add" : "update";
          extend(room, JSON.parse(response.body));
          // Remove the `currSize` for posterity.
          if ("currSize" in room) {
            delete room.currSize;
          }
          this.rooms.set(roomToken, room);

          eventEmitter.emit(eventName, room);
          callback(null, room);
        }, err => callback(err)).catch(err => callback(err));
    } else {
      callback(null, room);
    }
  },

  /**
   * Create a room.
   *
   * @param {Object}   room     Properties to be sent to the LoopServer
   * @param {Function} callback Function that will be invoked once the operation
   *                            finished. The first argument passed will be an
   *                            `Error` object or `null`. The second argument will
   *                            be the room, if it was created successfully.
   */
  create: function(room, callback) {
    if (!("roomName" in room) || !("expiresIn" in room) ||
        !("roomOwner" in room) || !("maxSize" in room)) {
      callback(new Error("Missing required property to create a room"));
      return;
    }

    let sessionType = MozLoopService.userProfile ? LOOP_SESSION_TYPE.FXA :
                      LOOP_SESSION_TYPE.GUEST;

    MozLoopService.hawkRequest(sessionType, "/rooms", "POST", room)
      .then(response => {
        let data = JSON.parse(response.body);
        extend(room, data);
        // Do not keep this value - it is a request to the server.
        delete room.expiresIn;
        this.rooms.set(room.roomToken, room);

        eventEmitter.emit("add", room);
        callback(null, room);
      }, error => callback(error)).catch(error => callback(error));
  },

  /**
   * Callback used to indicate changes to rooms data on the LoopServer.
   *
   * @param {String} version   Version number assigned to this change set.
   * @param {String} channelID Notification channel identifier.
   */
  onNotification: function(version, channelID) {
    gDirty = true;
    this.getAll(version, () => {});
  },
};
Object.freeze(LoopRoomsInternal);

/**
 * Public Loop Rooms API.
 *
 * LoopRooms implements the EventEmitter interface by exposing three methods -
 * `on`, `once` and `off` - to subscribe to events.
 * At this point the following events may be subscribed to:
 *  - 'add':       A new room object was successfully added to the data store.
 *  - 'remove':    A room was successfully removed from the data store.
 *  - 'update':    A room object was successfully updated with changed
 *                 properties in the data store.
 *
 * See the internal code for the API documentation.
 */
this.LoopRooms = {
  getAll: function(version, callback) {
    return LoopRoomsInternal.getAll(version, callback);
  },

  get: function(roomToken, callback) {
    return LoopRoomsInternal.get(roomToken, callback);
  },

  create: function(options, callback) {
    return LoopRoomsInternal.create(options, callback);
  },

  promise: function(method, ...params) {
    return new Promise((resolve, reject) => {
      this[method](...params, (error, result) => {
        if (error) {
          reject(error);
        } else {
          resolve(result);
        }
      });
    });
  },

  on: (...params) => eventEmitter.on(...params),

  once: (...params) => eventEmitter.once(...params),

  off: (...params) => eventEmitter.off(...params)
};
Object.freeze(this.LoopRooms);
