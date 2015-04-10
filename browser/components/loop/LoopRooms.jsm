/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
const {MozLoopService, LOOP_SESSION_TYPE} = Cu.import("resource:///modules/loop/MozLoopService.jsm", {});
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyGetter(this, "eventEmitter", function() {
  const {EventEmitter} = Cu.import("resource://gre/modules/devtools/event-emitter.js", {});
  return new EventEmitter();
});
XPCOMUtils.defineLazyGetter(this, "gLoopBundle", function() {
  return Services.strings.createBundle('chrome://browser/locale/loop/loop.properties');
});

this.EXPORTED_SYMBOLS = ["LoopRooms", "roomsPushNotification"];

// The maximum number of clients that we support currently.
const CLIENT_MAX_SIZE = 2;

const roomsPushNotification = function(version, channelID) {
  return LoopRoomsInternal.onNotification(version, channelID);
};

// Since the LoopRoomsInternal.rooms map as defined below is a local cache of
// room objects that are retrieved from the server, this is list may become out
// of date. The Push server may notify us of this event, which will set the global
// 'dirty' flag to TRUE.
let gDirty = true;
// Global variable that keeps track of the currently used account.
let gCurrentUser = null;

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
      eventEmitter.emit("joined", room, participant);
      eventEmitter.emit("joined:" + room.roomToken, participant);
    }
  }

  // Check for participants that left.
  for (participant of room.participants) {
    if (!containsParticipant(updatedRoom, participant)) {
      eventEmitter.emit("left", room, participant);
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
  /**
   * @var {Map} rooms Collection of rooms currently in cache.
   */
  rooms: new Map(),

  /**
   * @var {String} sessionType The type of user session. May be 'FXA' or 'GUEST'.
   */
  get sessionType() {
    return MozLoopService.userProfile ? LOOP_SESSION_TYPE.FXA :
                                        LOOP_SESSION_TYPE.GUEST;
  },

  /**
   * @var {Number} participantsCount The total amount of participants currently
   *                                 inside all rooms.
   */
  get participantsCount() {
    let count = 0;
    for (let room of this.rooms.values()) {
      if (room.deleted || !("participants" in room)) {
        continue;
      }
      count += room.participants.length;
    }
    return count;
  },

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
  getAll: function(version = null, callback = null) {
    if (!callback) {
      callback = version;
      version = null;
    }

    Task.spawn(function* () {
      if (!gDirty) {
        callback(null, [...this.rooms.values()]);
        return;
      }

      // Fetch the rooms from the server.
      let url = "/rooms" + (version ? "?version=" + encodeURIComponent(version) : "");
      let response = yield MozLoopService.hawkRequest(this.sessionType, url, "GET");
      let roomsList = JSON.parse(response.body);
      if (!Array.isArray(roomsList)) {
        throw new Error("Missing array of rooms in response.");
      }

      for (let room of roomsList) {
        // See if we already have this room in our cache.
        let orig = this.rooms.get(room.roomToken);

        if (room.deleted) {
          // If this client deleted the room, then we'll already have
          // deleted the room in the function below.
          if (orig) {
            this.rooms.delete(room.roomToken);
          }

          eventEmitter.emit("delete", room);
          eventEmitter.emit("delete:" + room.roomToken, room);
        } else {
          if (orig) {
            checkForParticipantsUpdate(orig, room);
          }
          // Remove the `currSize` for posterity.
          if ("currSize" in room) {
            delete room.currSize;
          }

          this.rooms.set(room.roomToken, room);

          let eventName = orig ? "update" : "add";
          eventEmitter.emit(eventName, room);
          eventEmitter.emit(eventName + ":" + room.roomToken, room);
        }
      }

      // If there's no rooms in the list, remove the guest created room flag, so that
      // we don't keep registering for guest when we don't need to.
      if (this.sessionType == LOOP_SESSION_TYPE.GUEST && !this.rooms.size) {
        this.setGuestCreatedRoom(false);
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

    MozLoopService.hawkRequest(this.sessionType, "/rooms/" + encodeURIComponent(roomToken), "GET")
      .then(response => {
        let data = JSON.parse(response.body);

        room.roomToken = roomToken;

        if (data.deleted) {
          this.rooms.delete(room.roomToken);

          extend(room, data);
          eventEmitter.emit("delete", room);
          eventEmitter.emit("delete:" + room.roomToken, room);
        } else {
          checkForParticipantsUpdate(room, data);
          extend(room, data);
          this.rooms.set(roomToken, room);

          let eventName = !needsUpdate ? "update" : "add";
          eventEmitter.emit(eventName, room);
          eventEmitter.emit(eventName + ":" + roomToken, room);
        }
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
    if (!("roomName" in room) || !("roomOwner" in room) ||
        !("maxSize" in room)) {
      callback(new Error("Missing required property to create a room"));
      return;
    }

    MozLoopService.hawkRequest(this.sessionType, "/rooms", "POST", room)
      .then(response => {
        let data = JSON.parse(response.body);
        extend(room, data);
        // Do not keep this value - it is a request to the server.
        delete room.expiresIn;
        this.rooms.set(room.roomToken, room);

        if (this.sessionType == LOOP_SESSION_TYPE.GUEST) {
          this.setGuestCreatedRoom(true);
        }

        eventEmitter.emit("add", room);
        callback(null, room);
      }, error => callback(error)).catch(error => callback(error));
  },

  /**
   * Sets whether or not the user has created a room in guest mode.
   *
   * @param {Boolean} created If the user has created the room.
   */
  setGuestCreatedRoom: function(created) {
    if (created) {
      Services.prefs.setBoolPref("loop.createdRoom", created);
    } else {
      Services.prefs.clearUserPref("loop.createdRoom");
    }
  },

  /**
   * Returns true if the user has a created room in guest mode.
   */
  getGuestCreatedRoom: function() {
    try {
      return Services.prefs.getBoolPref("loop.createdRoom");
    } catch (x) {
      return false;
    }
  },

  open: function(roomToken) {
    let windowData = {
      roomToken: roomToken,
      type: "room"
    };

    MozLoopService.openChatWindow(windowData);
  },

  /**
   * Deletes a room.
   *
   * @param {String}   roomToken The room token.
   * @param {Function} callback  Function that will be invoked once the operation
   *                             finished. The first argument passed will be an
   *                             `Error` object or `null`.
   */
  delete: function(roomToken, callback) {
    // XXX bug 1092954: Before deleting a room, the client should check room
    //     membership and forceDisconnect() all current participants.
    let room = this.rooms.get(roomToken);
    let url = "/rooms/" + encodeURIComponent(roomToken);
    MozLoopService.hawkRequest(this.sessionType, url, "DELETE")
      .then(response => {
        this.rooms.delete(roomToken);
        eventEmitter.emit("delete", room);
        eventEmitter.emit("delete:" + room.roomToken, room);
        callback(null, room);
      }, error => callback(error)).catch(error => callback(error));
  },

  /**
   * Internal function to handle POSTs to a room.
   *
   * @param {String} roomToken  The room token.
   * @param {Object} postData   The data to post to the room.
   * @param {Function} callback Function that will be invoked once the operation
   *                            finished. The first argument passed will be an
   *                            `Error` object or `null`.
   */
  _postToRoom(roomToken, postData, callback) {
    let url = "/rooms/" + encodeURIComponent(roomToken);
    MozLoopService.hawkRequest(this.sessionType, url, "POST", postData).then(response => {
      // Delete doesn't have a body return.
      var joinData = response.body ? JSON.parse(response.body) : {};
      callback(null, joinData);
    }, error => callback(error)).catch(error => callback(error));
  },

  /**
   * Joins a room
   *
   * @param {String} roomToken  The room token.
   * @param {Function} callback Function that will be invoked once the operation
   *                            finished. The first argument passed will be an
   *                            `Error` object or `null`.
   */
  join: function(roomToken, callback) {
    let displayName;
    if (MozLoopService.userProfile && MozLoopService.userProfile.email) {
      displayName = MozLoopService.userProfile.email;
    } else {
      displayName = gLoopBundle.GetStringFromName("display_name_guest");
    }

    this._postToRoom(roomToken, {
      action: "join",
      displayName: displayName,
      clientMaxSize: CLIENT_MAX_SIZE
    }, callback);
  },

  /**
   * Refreshes a room
   *
   * @param {String} roomToken    The room token.
   * @param {String} sessionToken The session token for the session that has been
   *                              joined
   * @param {Function} callback   Function that will be invoked once the operation
   *                              finished. The first argument passed will be an
   *                              `Error` object or `null`.
   */
  refreshMembership: function(roomToken, sessionToken, callback) {
    this._postToRoom(roomToken, {
      action: "refresh",
      sessionToken: sessionToken
    }, callback);
  },

  /**
   * Leaves a room. Although this is an sync function, no data is returned
   * from the server.
   *
   * @param {String} roomToken    The room token.
   * @param {String} sessionToken The session token for the session that has been
   *                              joined
   * @param {Function} callback   Optional. Function that will be invoked once the operation
   *                              finished. The first argument passed will be an
   *                              `Error` object or `null`.
   */
  leave: function(roomToken, sessionToken, callback) {
    if (!callback) {
      callback = function(error) {
        if (error) {
          MozLoopService.log.error(error);
        }
      };
    }
    this._postToRoom(roomToken, {
      action: "leave",
      sessionToken: sessionToken
    }, callback);
  },

  /**
   * Renames a room.
   *
   * @param {String} roomToken   The room token
   * @param {String} newRoomName The new name for the room
   * @param {Function} callback   Function that will be invoked once the operation
   *                              finished. The first argument passed will be an
   *                              `Error` object or `null`.
   */
  rename: function(roomToken, newRoomName, callback) {
    let room = this.rooms.get(roomToken);
    let url = "/rooms/" + encodeURIComponent(roomToken);

    let origRoom = this.rooms.get(roomToken);
    let patchData = {
      roomName: newRoomName
    };
    MozLoopService.hawkRequest(this.sessionType, url, "PATCH", patchData)
      .then(response => {
        let data = JSON.parse(response.body);
        extend(room, data);
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
    // See if we received a notification for the channel that's currently active:
    let channelIDs = MozLoopService.channelIDs;
    if ((this.sessionType == LOOP_SESSION_TYPE.GUEST && channelID != channelIDs.roomsGuest) ||
        (this.sessionType == LOOP_SESSION_TYPE.FXA   && channelID != channelIDs.roomsFxA)) {
      return;
    }

    gDirty = true;
    this.getAll(version, () => {});
  },

  /**
   * When a user logs in or out, this method should be invoked to check whether
   * the rooms cache needs to be refreshed.
   *
   * @param {String|null} user The FxA userID or NULL
   */
  maybeRefresh: function(user = null) {
    if (gCurrentUser == user) {
      return;
    }

    gCurrentUser = user;
    if (!gDirty) {
      gDirty = true;
      this.rooms.clear();
      eventEmitter.emit("refresh");
      this.getAll(null, () => {});
    }
  }
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
 *  - 'delete[:{room-id}]': A room was successfully removed from the data store.
 *  - 'update[:{room-id}]': A room object was successfully updated with changed
 *                          properties in the data store.
 *  - 'joined[:{room-id}]': A participant joined a room.
 *  - 'left[:{room-id}]':   A participant left a room.
 *
 * See the internal code for the API documentation.
 */
this.LoopRooms = {
  get participantsCount() {
    return LoopRoomsInternal.participantsCount;
  },

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

  delete: function(roomToken, callback) {
    return LoopRoomsInternal.delete(roomToken, callback);
  },

  join: function(roomToken, callback) {
    return LoopRoomsInternal.join(roomToken, callback);
  },

  refreshMembership: function(roomToken, sessionToken, callback) {
    return LoopRoomsInternal.refreshMembership(roomToken, sessionToken,
      callback);
  },

  leave: function(roomToken, sessionToken, callback) {
    return LoopRoomsInternal.leave(roomToken, sessionToken, callback);
  },

  rename: function(roomToken, newRoomName, callback) {
    return LoopRoomsInternal.rename(roomToken, newRoomName, callback);
  },

  getGuestCreatedRoom: function() {
    return LoopRoomsInternal.getGuestCreatedRoom();
  },

  maybeRefresh: function(user) {
    return LoopRoomsInternal.maybeRefresh(user);
  },

  /**
   * This method is only useful for unit tests to set the rooms cache to contain
   * a list of fake room data that can be asserted in tests.
   *
   * @param {Map} stub Stub cache containing fake rooms data
   */
  stubCache: function(stub) {
    LoopRoomsInternal.rooms.clear();
    if (stub) {
      // Fill up the rooms cache with room objects provided in the `stub` Map.
      for (let [key, value] of stub.entries()) {
        LoopRoomsInternal.rooms.set(key, value);
      }
      gDirty = false;
    } else {
      // Restore the cache to not be stubbed anymore, but it'll need a refresh
      // from the server for sure.
      gDirty = true;
    }
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
