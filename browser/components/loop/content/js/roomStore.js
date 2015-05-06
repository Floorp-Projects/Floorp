/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.store = loop.store || {};

(function(mozL10n) {
  "use strict";

  /**
   * Shared actions.
   * @type {Object}
   */
  var sharedActions = loop.shared.actions;

  /**
   * Maximum size given to createRoom; only 2 is supported (and is
   * always passed) because that's what the user-experience is currently
   * designed and tested to handle.
   * @type {Number}
   */
  var MAX_ROOM_CREATION_SIZE = loop.store.MAX_ROOM_CREATION_SIZE = 2;

  /**
   * Room validation schema. See validate.js.
   * @type {Object}
   */
  var roomSchema = {
    roomToken:    String,
    roomUrl:      String,
    // roomName:     String - Optional.
    // roomKey:      String - Optional.
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
   * @param {loop.Dispatcher} dispatcher  The dispatcher for dispatching actions
   *                                      and registering to consume actions.
   * @param {Object} options Options object:
   * - {mozLoop}         mozLoop          The MozLoop API object.
   * - {ActiveRoomStore} activeRoomStore  An optional substore for active room
   *                                      state.
   * - {Notifications}   notifications    An optional notifications item that is
   *                                      required if create actions are to be used
   */
  loop.store.RoomStore = loop.store.createStore({
    /**
     * Maximum size given to createRoom; only 2 is supported (and is
     * always passed) because that's what the user-experience is currently
     * designed and tested to handle.
     * @type {Number}
     */
    maxRoomCreationSize: MAX_ROOM_CREATION_SIZE,

    /**
     * Registered actions.
     * @type {Array}
     */
    actions: [
      "addSocialShareButton",
      "addSocialShareProvider",
      "createRoom",
      "createdRoom",
      "createRoomError",
      "copyRoomUrl",
      "deleteRoom",
      "deleteRoomError",
      "emailRoomUrl",
      "getAllRooms",
      "getAllRoomsError",
      "openRoom",
      "renameRoom",
      "renameRoomError",
      "shareRoomUrl",
      "updateRoomList"
    ],

    initialize: function(options) {
      if (!options.mozLoop) {
        throw new Error("Missing option mozLoop");
      }
      this._mozLoop = options.mozLoop;
      this._notifications = options.notifications;

      if (options.activeRoomStore) {
        this.activeRoomStore = options.activeRoomStore;
        this.activeRoomStore.on("change",
                                this._onActiveRoomStoreChange.bind(this));
      }
    },

    getInitialStoreState: function() {
      return {
        activeRoom: this.activeRoomStore ? this.activeRoomStore.getStoreState() : {},
        error: null,
        pendingCreation: false,
        pendingInitialRetrieval: false,
        rooms: []
      };
    },

    /**
     * Registers mozLoop.rooms events.
     */
    startListeningToRoomEvents: function() {
      // Rooms event registration
      this._mozLoop.rooms.on("add", this._onRoomAdded.bind(this));
      this._mozLoop.rooms.on("update", this._onRoomUpdated.bind(this));
      this._mozLoop.rooms.on("delete", this._onRoomRemoved.bind(this));
      this._mozLoop.rooms.on("refresh", this._onRoomsRefresh.bind(this));
    },

    /**
     * Updates active room store state.
     */
    _onActiveRoomStoreChange: function() {
      this.setStoreState({activeRoom: this.activeRoomStore.getStoreState()});
    },

    /**
     * Updates current room list when a new room is available.
     *
     * @param {String} eventName     The event name (unused).
     * @param {Object} addedRoomData The added room data.
     */
    _onRoomAdded: function(eventName, addedRoomData) {
      addedRoomData.participants = addedRoomData.participants || [];
      addedRoomData.ctime = addedRoomData.ctime || new Date().getTime();
      this.dispatchAction(new sharedActions.UpdateRoomList({
        // Ensure the room isn't part of the list already, then add it.
        roomList: this._storeState.rooms.filter(function(room) {
          return addedRoomData.roomToken !== room.roomToken;
        }).concat(new Room(addedRoomData))
      }));
    },

    /**
     * Executed when a room is updated.
     *
     * @param {String} eventName       The event name (unused).
     * @param {Object} updatedRoomData The updated room data.
     */
    _onRoomUpdated: function(eventName, updatedRoomData) {
      this.dispatchAction(new sharedActions.UpdateRoomList({
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
      this.dispatchAction(new sharedActions.UpdateRoomList({
        roomList: this._storeState.rooms.filter(function(room) {
          return room.roomToken !== removedRoomData.roomToken;
        })
      }));
    },

    /**
     * Executed when the user switches accounts.
     *
     * @param {String} eventName The event name (unused).
     */
    _onRoomsRefresh: function(eventName) {
      this.dispatchAction(new sharedActions.UpdateRoomList({
        roomList: []
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
        var match = searchRegExp.exec(room.decryptedContext.roomName);
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
      this.setStoreState({
        pendingCreation: true,
        error: null
      });

      var roomCreationData = {
        decryptedContext: {
          roomName:  this._generateNewRoomName(actionData.nameTemplate)
        },
        roomOwner: actionData.roomOwner,
        maxSize:   this.maxRoomCreationSize
      };

      if ("urls" in actionData) {
        roomCreationData.decryptedContext.urls = actionData.urls;
      }

      this._notifications.remove("create-room-error");

      this._mozLoop.rooms.create(roomCreationData, function(err, createdRoom) {
        if (err) {
          this.dispatchAction(new sharedActions.CreateRoomError({error: err}));
          return;
        }

        this.dispatchAction(new sharedActions.CreatedRoom({
          roomToken: createdRoom.roomToken
        }));
      }.bind(this));
    },

    /**
     * Executed when a room has been created
     */
    createdRoom: function(actionData) {
      this.setStoreState({pendingCreation: false});

      // Opens the newly created room
      this.dispatchAction(new sharedActions.OpenRoom({
        roomToken: actionData.roomToken
      }));
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

      // XXX Needs a more descriptive error - bug 1109151.
      this._notifications.set({
        id: "create-room-error",
        level: "error",
        message: mozL10n.get("generic_failure_title")
      });
    },

    /**
     * Copy a room url.
     *
     * @param  {sharedActions.CopyRoomUrl} actionData The action data.
     */
    copyRoomUrl: function(actionData) {
      this._mozLoop.copyString(actionData.roomUrl);
      this._mozLoop.notifyUITour("Loop:RoomURLCopied");
    },

    /**
     * Emails a room url.
     *
     * @param  {sharedActions.EmailRoomUrl} actionData The action data.
     */
    emailRoomUrl: function(actionData) {
      loop.shared.utils.composeCallUrlEmail(actionData.roomUrl);
      this._mozLoop.notifyUITour("Loop:RoomURLEmailed");
    },

    /**
     * Share a room url.
     *
     * @param  {sharedActions.ShareRoomUrl} actionData The action data.
     */
    shareRoomUrl: function(actionData) {
      var providerOrigin = new URL(actionData.provider.origin).hostname;
      var shareTitle = "";
      var shareBody = null;

      switch (providerOrigin) {
        case "mail.google.com":
          shareTitle = mozL10n.get("share_email_subject5", {
            clientShortname2: mozL10n.get("clientShortname2")
          });
          shareBody = mozL10n.get("share_email_body5", {
            callUrl: actionData.roomUrl,
            brandShortname: mozL10n.get("brandShortname"),
            clientShortname2: mozL10n.get("clientShortname2"),
            clientSuperShortname: mozL10n.get("clientSuperShortname"),
            learnMoreUrl: this._mozLoop.getLoopPref("learnMoreUrl")
          });
          break;
        case "twitter.com":
        default:
          shareTitle = mozL10n.get("share_tweet", {
            clientShortname2: mozL10n.get("clientShortname2")
          });
          break;
      }

      this._mozLoop.socialShareRoom(actionData.provider.origin, actionData.roomUrl,
        shareTitle, shareBody);
      this._mozLoop.notifyUITour("Loop:RoomURLShared");
    },

    /**
     * Add the Social Share button to the browser toolbar.
     *
     * @param {sharedActions.AddSocialShareButton} actionData The action data.
     */
    addSocialShareButton: function(actionData) {
      this._mozLoop.addSocialShareButton();
    },

    /**
     * Open the share panel to add a Social share provider.
     *
     * @param {sharedActions.AddSocialShareProvider} actionData The action data.
     */
    addSocialShareProvider: function(actionData) {
      this._mozLoop.addSocialShareProvider();
    },

    /**
     * Creates a new room.
     *
     * @param {sharedActions.DeleteRoom} actionData The action data.
     */
    deleteRoom: function(actionData) {
      this._mozLoop.rooms.delete(actionData.roomToken, function(err) {
        if (err) {
         this.dispatchAction(new sharedActions.DeleteRoomError({error: err}));
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

        this.dispatchAction(action);

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
    },

    /**
     * Renames a room.
     *
     * @param {sharedActions.RenameRoom} actionData
     */
    renameRoom: function(actionData) {
      var oldRoomName = this.getStoreState("roomName");
      var newRoomName = actionData.newRoomName.trim();

      // Skip update if name is unchanged or empty.
      if (!newRoomName || oldRoomName === newRoomName) {
        return;
      }

      this.setStoreState({error: null});
      this._mozLoop.rooms.rename(actionData.roomToken, newRoomName,
        function(err) {
          if (err) {
            this.dispatchAction(new sharedActions.RenameRoomError({error: err}));
          }
        }.bind(this));
    },

    renameRoomError: function(actionData) {
      this.setStoreState({error: actionData.error});
    }
  });
})(document.mozL10n || navigator.mozL10n);
