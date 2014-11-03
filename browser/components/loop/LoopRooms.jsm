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
 * Checks whether a participant is already part of a room.
 *
 * @see https://wiki.mozilla.org/Loop/Architecture/Rooms#User_Identification_in_a_Room
 *
 * @param {Object} room        A room object that contains a list of current participants
 * @param {Object} participant Participant to check if it's already there
 * @returns {Boolean} TRUE when the participant is already a member of the room,
 *                    FALSE when it's not.
 */
const containsParticipant = function(room, participant) {
  for (let user of room.participants) {
    if (user.roomConnectionId == participant.roomConnectionId) {
      return true;
    }
  }
  return false;
};

/**
 * Compares the list of participants of the room currently in the cache and an
 * updated version of that room. When a new participant is found, the 'joined'
 * event is emitted. When a participant is not found in the update, it emits a
 * 'left' event.
 *
 * @param {Object} room        A room object to compare the participants list
 *                             against
 * @param {Object} updatedRoom A room object that contains the most up-to-date
 *                             list of participants
 */
const checkForParticipantsUpdate = function(room, updatedRoom) {
  // Partially fetched rooms don't contain the participants list yet. Skip the
  // check for now.
  if (!("participants" in room)) {
    return;
  }

  let participant;
  // Check for participants that joined.
  for (participant of updatedRoom.participants) {
    if (!containsParticipant(room, participant)) {
      eventEmitter.emit("joined", room.roomToken, participant);
      eventEmitter.emit("joined:" + room.roomToken, participant);
    }
  }

  // Check for participants that left.
  for (participant of room.participants) {
    if (!containsParticipant(updatedRoom, participant)) {
      eventEmitter.emit("left", room.roomToken, participant);
      eventEmitter.emit("left:" + room.roomToken, participant);
    }
  }
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
      yield MozLoopService.promiseRegisteredWithServers();

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

      for (let room of roomsList) {
        // See if we already have this room in our cache.
        let orig = this.rooms.get(room.roomToken);
        if (orig) {
          checkForParticipantsUpdate(orig, room);
        }
        this.rooms.set(room.roomToken, room);
        // When a version is specified, all the data is already provided by this
        // request.
        if (version) {
          eventEmitter.emit("update", room);
          eventEmitter.emit("update" + ":" + room.roomToken, room);
        } else {
          // Next, request the detailed information for each room. If the request
          // fails the room data will not be added to the map.
          yield LoopRooms.promise("get", room.roomToken);
        }
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
    let needsUpdate = !("participants" in room);
    if (!gDirty && !needsUpdate) {
      // Dirty flag is not set AND the necessary data is available, so we can
      // simply return the room.
      callback(null, room);
      return;
    }

    let sessionType = MozLoopService.userProfile ? LOOP_SESSION_TYPE.FXA :
                      LOOP_SESSION_TYPE.GUEST;
    MozLoopService.hawkRequest(sessionType, "/rooms/" + encodeURIComponent(roomToken), "GET")
      .then(response => {
        let data = JSON.parse(response.body);

        room.roomToken = roomToken;
        checkForParticipantsUpdate(room, data);
        extend(room, data);

        // Remove the `currSize` for posterity.
        if ("currSize" in room) {
          delete room.currSize;
        }
        this.rooms.set(roomToken, room);

        let eventName = !needsUpdate ? "update" : "add";
        eventEmitter.emit(eventName, room);
        eventEmitter.emit(eventName + ":" + roomToken, room);
        callback(null, room);
      }, err => callback(err)).catch(err => callback(err));
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

  open: function(roomToken) {
    let windowData = {
      roomToken: roomToken,
      type: "room"
    };

    MozLoopService.openChatWindow(windowData);
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
 *  - 'add[:{room-id}]':    A new room object was successfully added to the data
 *                          store.
 *  - 'remove[:{room-id}]': A room was successfully removed from the data store.
 *  - 'update[:{room-id}]': A room object was successfully updated with changed
 *                          properties in the data store.
 *  - 'joined[:{room-id}]': A participant joined a room.
 *  - 'left[:{room-id}]':   A participant left a room.
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

  open: function(roomToken) {
    return LoopRoomsInternal.open(roomToken);
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
