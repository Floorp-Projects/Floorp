/* This Source Code Form is subject to the terms of the Mozilla Public
      +  * License, v. 2.0. If a copy of the MPL was not distributed with this
      +  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci } = require("chrome");

const { LongStringActor } = require("devtools/server/actors/string");

const {
  TYPES: { SERVER_SENT_EVENT },
} = require("devtools/server/actors/resources/index");

const eventSourceEventService = Cc[
  "@mozilla.org/eventsourceevent/service;1"
].getService(Ci.nsIEventSourceEventService);

class ServerSentEventWatcher {
  constructor() {
    this.windowIds = new Set();
    // Register for backend events.
    this.onWindowReady = this.onWindowReady.bind(this);
    this.onWindowDestroy = this.onWindowDestroy.bind(this);
  }
  /**
   * Start watching for all server sent events related to a given Target Actor.
   *
   * @param TargetActor targetActor
   *        The target actor on which we should observe server sent events.
   * @param Object options
   *        Dictionary object with following attributes:
   *        - onAvailable: mandatory function
   *          This will be called for each resource.
   */
  watch(targetActor, { onAvailable }) {
    this.onAvailable = onAvailable;
    this.targetActor = targetActor;

    for (const window of this.targetActor.windows) {
      const { innerWindowId } = window.windowGlobalChild;
      this.startListening(innerWindowId);
    }

    // Listen for subsequent top-level-document reloads/navigations,
    // new iframe additions or current iframe reloads/navigation.
    this.targetActor.on("window-ready", this.onWindowReady);
    this.targetActor.on("window-destroyed", this.onWindowDestroy);
  }

  static createResource(messageType, eventParams) {
    return {
      resourceType: SERVER_SENT_EVENT,
      messageType,
      ...eventParams,
    };
  }

  static prepareFramePayload(targetActor, frame) {
    const payload = new LongStringActor(targetActor.conn, frame);
    targetActor.manage(payload);
    return payload.form();
  }

  onWindowReady({ window }) {
    const { innerWindowId } = window.windowGlobalChild;
    this.startListening(innerWindowId);
  }

  onWindowDestroy({ id }) {
    this.stopListening(id);
  }

  startListening(innerWindowId) {
    if (!this.windowIds.has(innerWindowId)) {
      this.windowIds.add(innerWindowId);
      eventSourceEventService.addListener(innerWindowId, this);
    }
  }

  stopListening(innerWindowId) {
    if (this.windowIds.has(innerWindowId)) {
      this.windowIds.delete(innerWindowId);
      // The listener might have already been cleaned up on `window-destroy`.
      if (!eventSourceEventService.hasListenerFor(innerWindowId)) {
        console.warn(
          "Already stopped listening to server sent events for this window."
        );
        return;
      }
      eventSourceEventService.removeListener(innerWindowId, this);
    }
  }

  destroy() {
    // cleanup any other listeners not removed on `window-destroy`
    for (const id of this.windowIds) {
      this.stopListening(id);
    }
    this.targetActor.off("window-ready", this.onWindowReady);
    this.targetActor.off("window-destroyed", this.onWindowDestroy);
  }

  // nsIEventSourceEventService specific functions
  eventSourceConnectionOpened(httpChannelId) {}

  eventSourceConnectionClosed(httpChannelId) {
    const resource = ServerSentEventWatcher.createResource(
      "eventSourceConnectionClosed",
      { httpChannelId }
    );
    this.onAvailable([resource]);
  }

  eventReceived(httpChannelId, eventName, lastEventId, data, retry, timeStamp) {
    const payload = ServerSentEventWatcher.prepareFramePayload(
      this.targetActor,
      data
    );
    const resource = ServerSentEventWatcher.createResource("eventReceived", {
      httpChannelId,
      data: {
        payload,
        eventName,
        lastEventId,
        retry,
        timeStamp,
      },
    });

    this.onAvailable([resource]);
  }
}

module.exports = ServerSentEventWatcher;
