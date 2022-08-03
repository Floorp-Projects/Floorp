/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci } = require("chrome");
const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const { webSocketSpec } = require("devtools/shared/specs/websocket");
const { LongStringActor } = require("devtools/server/actors/string");

const webSocketEventService = Cc[
  "@mozilla.org/websocketevent/service;1"
].getService(Ci.nsIWebSocketEventService);

/**
 * This actor intercepts WebSocket traffic for a specific window.
 *
 * @see devtools/shared/spec/websocket.js for documentation.
 */
const WebSocketActor = ActorClassWithSpec(webSocketSpec, {
  initialize(conn, targetActor) {
    Actor.prototype.initialize.call(this, conn);

    this.targetActor = targetActor;
    this.innerWindowID = null;

    // Each connection's webSocketSerialID is mapped to a httpChannelId
    this.connections = new Map();

    // Register for backend events.
    this.onWindowReady = this.onWindowReady.bind(this);
    this.targetActor.on("window-ready", this.onWindowReady);
  },

  onWindowReady({ isTopLevel }) {
    if (isTopLevel) {
      this.startListening();
    }
  },

  destroy() {
    this.targetActor.off("window-ready", this.onWindowReady);

    this.stopListening();
    Actor.prototype.destroy.call(this);
  },

  // Actor API

  startListening() {
    this.stopListening();
    this.innerWindowID = this.targetActor.window.windowGlobalChild.innerWindowId;
    webSocketEventService.addListener(this.innerWindowID, this);
  },

  stopListening() {
    if (!this.innerWindowID) {
      return;
    }
    if (webSocketEventService.hasListenerFor(this.innerWindowID)) {
      webSocketEventService.removeListener(this.innerWindowID, this);
    }

    this.innerWindowID = null;
  },

  // nsIWebSocketEventService

  webSocketCreated(webSocketSerialID, uri, protocols) {},

  webSocketOpened(
    webSocketSerialID,
    effectiveURI,
    protocols,
    extensions,
    httpChannelId
  ) {
    this.connections.set(webSocketSerialID, httpChannelId);

    this.emit(
      "serverWebSocketOpened",
      httpChannelId,
      effectiveURI,
      protocols,
      extensions
    );
  },

  webSocketMessageAvailable(webSocketSerialID, data, messageType) {},

  webSocketClosed(webSocketSerialID, wasClean, code, reason) {
    const httpChannelId = this.connections.get(webSocketSerialID);

    this.connections.delete(webSocketSerialID);
    this.emit("serverWebSocketClosed", httpChannelId, wasClean, code, reason);
  },

  frameReceived(webSocketSerialID, frame) {
    const httpChannelId = this.connections.get(webSocketSerialID);

    if (!httpChannelId) {
      return;
    }

    let payload = frame.payload;
    payload = new LongStringActor(this.conn, payload);
    this.manage(payload);
    payload = payload.form();

    this.emit("serverFrameReceived", httpChannelId, {
      type: "received",
      payload,
      timeStamp: frame.timeStamp,
      finBit: frame.finBit,
      rsvBit1: frame.rsvBit1,
      rsvBit2: frame.rsvBit2,
      rsvBit3: frame.rsvBit3,
      opCode: frame.opCode,
      mask: frame.mask,
      maskBit: frame.maskBit,
    });
  },

  frameSent(webSocketSerialID, frame) {
    const httpChannelId = this.connections.get(webSocketSerialID);

    if (!httpChannelId) {
      return;
    }

    let payload = new LongStringActor(this.conn, frame.payload);
    this.manage(payload);
    payload = payload.form();

    this.emit("serverFrameSent", httpChannelId, {
      type: "sent",
      payload,
      timeStamp: frame.timeStamp,
      finBit: frame.finBit,
      rsvBit1: frame.rsvBit1,
      rsvBit2: frame.rsvBit2,
      rsvBit3: frame.rsvBit3,
      opCode: frame.opCode,
      mask: frame.mask,
      maskBit: frame.maskBit,
    });
  },
});

exports.WebSocketActor = WebSocketActor;
