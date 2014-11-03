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
    /**
     * Get the window data for the provided window id
     */
    GetWindowData: Action.define("getWindowData", {
      windowId: String
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
     * Used to indicate the peer hung up the call.
     */
    PeerHungupCall: Action.define("peerHungupCall", {
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
    })
  };
})();
