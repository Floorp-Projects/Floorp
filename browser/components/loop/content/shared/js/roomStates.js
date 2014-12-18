/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.store = loop.store || {};

loop.store.ROOM_STATES = {
    // The initial state of the room
    INIT: "room-init",
    // The store is gathering the room data
    GATHER: "room-gather",
    // The store has got the room data
    READY: "room-ready",
    // Obtaining media from the user
    MEDIA_WAIT: "room-media-wait",
    // The room is known to be joined on the loop-server
    JOINED: "room-joined",
    // The room is connected to the sdk server.
    SESSION_CONNECTED: "room-session-connected",
    // There are participants in the room.
    HAS_PARTICIPANTS: "room-has-participants",
    // There was an issue with the room
    FAILED: "room-failed",
    // The room is full
    FULL: "room-full",
    // The room conversation has ended
    ENDED: "room-ended",
    // The window is closing
    CLOSING: "room-closing"
};
