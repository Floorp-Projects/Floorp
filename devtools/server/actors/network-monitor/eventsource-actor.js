/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci } = require("chrome");
const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const { eventSourceSpec } = require("devtools/shared/specs/eventsource");
const { LongStringActor } = require("devtools/server/actors/string");

const eventSourceEventService = Cc[
  "@mozilla.org/eventsourceevent/service;1"
].getService(Ci.nsIEventSourceEventService);

/**
 * This actor intercepts EventSource traffic for a specific window.
 * @see devtools/shared/spec/eventsource.js for documentation.
 */
const EventSourceActor = ActorClassWithSpec(eventSourceSpec, {
  initialize(conn, targetActor) {
    Actor.prototype.initialize.call(this, conn);

    this.targetActor = targetActor;
    this.innerWindowID = null;

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

  // Actor API.

  startListening() {
    this.stopListening();
    this.innerWindowID = this.targetActor.window.windowGlobalChild.innerWindowId;
    eventSourceEventService.addListener(this.innerWindowID, this);
  },

  stopListening() {
    if (!this.innerWindowID) {
      return;
    }
    if (eventSourceEventService.hasListenerFor(this.innerWindowID)) {
      eventSourceEventService.removeListener(this.innerWindowID, this);
    }

    this.innerWindowID = null;
  },

  // Implement functions of nsIEventSourceEventService.

  eventSourceConnectionOpened(httpChannelId) {},

  eventSourceConnectionClosed(httpChannelId) {
    this.emit("serverEventSourceConnectionClosed", httpChannelId);
  },

  eventReceived(httpChannelId, eventName, lastEventId, data, retry, timeStamp) {
    let payload = new LongStringActor(this.conn, data);
    this.manage(payload);
    payload = payload.form();
    this.emit("serverEventReceived", httpChannelId, {
      payload,
      eventName,
      lastEventId,
      retry,
      timeStamp,
    });
  },
});

exports.EventSourceActor = EventSourceActor;
