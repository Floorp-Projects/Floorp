/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.shared = loop.shared || {};
loop.shared.actions = (function() {
  "use strict";

  /**
   * Actions are events that are triggered by the user, e.g. clicking a button,
   * or by an async event, e.g. status received.
   *
   * They should be dispatched to stores via the dispatcher.
   */

  function Action(name, schema, values) {
    var validatedData = new loop.validate.Validator(schema || {})
                                         .validate(values || {});
    for (var prop in validatedData)
      this[prop] = validatedData[prop];

    this.name = name;
  }

  Action.define = function(name, schema) {
    return Action.bind(null, name, schema);
  };

  return {
    Action: Action,

    /**
     * Get the window data for the provided window id
     */
    GetWindowData: Action.define("getWindowData", {
      windowId: String
    }),

    /**
     * Extract the token information and type for the standalone window
     */
    ExtractTokenInfo: Action.define("extractTokenInfo", {
      windowPath: String
    }),

    /**
     * Used to pass round the window data so that stores can
     * record the appropriate data.
     */
    SetupWindowData: Action.define("setupWindowData", {
      windowId: String,
      type: String

      // Optional Items. There are other optional items typically sent
      // around with this action. They are for the setup of calls and rooms and
      // depend on the type. See LoopCalls and LoopRooms for the details of this
      // data.
    }),

    /**
     * Used to fetch the data from the server for a room or call for the
     * token.
     */
    FetchServerData: Action.define("fetchServerData", {
      token: String,
      windowType: String
    }),

    /**
     * Used to signal when the window is being unloaded.
     */
    WindowUnload: Action.define("windowUnload", {
    }),

    /**
     * Fetch a new call url from the server, intended to be sent over email when
     * a contact can't be reached.
     */
    FetchEmailLink: Action.define("fetchEmailLink", {
    }),

    /**
     * Used to cancel call setup.
     */
    CancelCall: Action.define("cancelCall", {
    }),

    /**
     * Used to retry a failed call.
     */
    RetryCall: Action.define("retryCall", {
    }),

    /**
     * Used to initiate connecting of a call with the relevant
     * sessionData.
     */
    ConnectCall: Action.define("connectCall", {
      // This object contains the necessary details for the
      // connection of the websocket, and the SDK
      sessionData: Object
    }),

    /**
     * Used for hanging up the call at the end of a successful call.
     */
    HangupCall: Action.define("hangupCall", {
    }),

    /**
     * Used to indicate the remote peer was disconnected for some reason.
     *
     * peerHungup is true if the peer intentionally disconnected, false otherwise.
     */
    RemotePeerDisconnected: Action.define("remotePeerDisconnected", {
      peerHungup: Boolean
    }),

    /**
     * Used for notifying of connection progress state changes.
     * The connection refers to the overall connection flow as indicated
     * on the websocket.
     */
    ConnectionProgress: Action.define("connectionProgress", {
      // The connection state from the websocket.
      wsState: String
    }),

    /**
     * Used for notifying of connection failures.
     */
    ConnectionFailure: Action.define("connectionFailure", {
      // A string relating to the reason the connection failed.
      reason: String
    }),

    /**
     * Used to notify that the sdk session is now connected to the servers.
     */
    ConnectedToSdkServers: Action.define("connectedToSdkServers", {
    }),

    /**
     * Used to notify that a remote peer has connected to the room.
     */
    RemotePeerConnected: Action.define("remotePeerConnected", {
    }),

    /**
     * Used by the ongoing views to notify stores about the elements
     * required for the sdk.
     */
    SetupStreamElements: Action.define("setupStreamElements", {
      // The configuration for the publisher/subscribe options
      publisherConfig: Object,
      // The local stream element
      getLocalElementFunc: Function,
      // The remote stream element
      getRemoteElementFunc: Function
    }),

    /**
     * Used for notifying that the media is now up for the call.
     */
    MediaConnected: Action.define("mediaConnected", {
    }),

    /**
     * Used to mute or unmute a stream
     */
    SetMute: Action.define("setMute", {
      // The part of the stream to enable, e.g. "audio" or "video"
      type: String,
      // Whether or not to enable the stream.
      enabled: Boolean
    }),

    /**
     * Creates a new room.
     * XXX: should move to some roomActions module - refs bug 1079284
     */
    CreateRoom: Action.define("createRoom", {
      // The localized template to use to name the new room
      // (eg. "Conversation {{conversationLabel}}").
      nameTemplate: String,
      roomOwner: String
    }),

    /**
     * Rooms creation error.
     * XXX: should move to some roomActions module - refs bug 1079284
     */
    CreateRoomError: Action.define("createRoomError", {
      error: Error
    }),

    /**
     * Deletes a room.
     * XXX: should move to some roomActions module - refs bug 1079284
     */
    DeleteRoom: Action.define("deleteRoom", {
      roomToken: String
    }),

    /**
     * Room deletion error.
     * XXX: should move to some roomActions module - refs bug 1079284
     */
    DeleteRoomError: Action.define("deleteRoomError", {
      error: Error
    }),

    /**
     * Retrieves room list.
     * XXX: should move to some roomActions module - refs bug 1079284
     */
    GetAllRooms: Action.define("getAllRooms", {
    }),

    /**
     * An error occured while trying to fetch the room list.
     * XXX: should move to some roomActions module - refs bug 1079284
     */
    GetAllRoomsError: Action.define("getAllRoomsError", {
      error: Error
    }),

    /**
     * Updates room list.
     * XXX: should move to some roomActions module - refs bug 1079284
     */
    UpdateRoomList: Action.define("updateRoomList", {
      roomList: Array
    }),

    /**
     * Opens a room.
     * XXX: should move to some roomActions module - refs bug 1079284
     */
    OpenRoom: Action.define("openRoom", {
      roomToken: String
    }),

    /**
     * Renames a room.
     * XXX: should move to some roomActions module - refs bug 1079284
     */
    RenameRoom: Action.define("renameRoom", {
      roomToken: String,
      newRoomName: String
    }),

    /**
     * Copy a room url into the user's clipboard.
     * XXX: should move to some roomActions module - refs bug 1079284
     */
    CopyRoomUrl: Action.define("copyRoomUrl", {
      roomUrl: String
    }),

    /**
     * Email a room url.
     * XXX: should move to some roomActions module - refs bug 1079284
     */
    EmailRoomUrl: Action.define("emailRoomUrl", {
      roomUrl: String
    }),

    /**
     * XXX: should move to some roomActions module - refs bug 1079284
     */
    RoomFailure: Action.define("roomFailure", {
      error: Object
    }),

    /**
     * Sets up the room information when it is received.
     * XXX: should move to some roomActions module - refs bug 1079284
     *
     * @see https://wiki.mozilla.org/Loop/Architecture/Rooms#GET_.2Frooms.2F.7Btoken.7D
     */
    SetupRoomInfo: Action.define("setupRoomInfo", {
      roomName: String,
      roomOwner: String,
      roomToken: String,
      roomUrl: String
    }),

    /**
     * Updates the room information when it is received.
     * XXX: should move to some roomActions module - refs bug 1079284
     *
     * @see https://wiki.mozilla.org/Loop/Architecture/Rooms#GET_.2Frooms.2F.7Btoken.7D
     */
    UpdateRoomInfo: Action.define("updateRoomInfo", {
      roomName: String,
      roomOwner: String,
      roomUrl: String
    }),

    /**
     * Starts the process for the user to join the room.
     * XXX: should move to some roomActions module - refs bug 1079284
     */
    JoinRoom: Action.define("joinRoom", {
    }),

    /**
     * Signals the user has successfully joined the room on the loop-server.
     * XXX: should move to some roomActions module - refs bug 1079284
     *
     * @see https://wiki.mozilla.org/Loop/Architecture/Rooms#Joining_a_Room
     */
    JoinedRoom: Action.define("joinedRoom", {
      apiKey: String,
      sessionToken: String,
      sessionId: String,
      expires: Number
    }),

    /**
     * Used to indicate the user wishes to leave the room.
     */
    LeaveRoom: Action.define("leaveRoom", {
    })
  };
})();
