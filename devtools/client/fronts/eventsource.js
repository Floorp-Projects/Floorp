/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const { eventSourceSpec } = require("devtools/shared/specs/eventsource");

/**
 * A EventSourceFront is used as a front end for the EventSourceActor that is
 * created on the server, hiding implementation details.
 */
class EventSourceFront extends FrontClassWithSpec(eventSourceSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    this._onEventSourceConnectionClosed = this._onEventSourceConnectionClosed.bind(
      this
    );
    this._onEventReceived = this._onEventReceived.bind(this);

    // Attribute name from which to retrieve the actorID
    // out of the target actor's form
    this.formAttributeName = "eventSourceActor";

    this.on(
      "serverEventSourceConnectionClosed",
      this._onEventSourceConnectionClosed
    );
    this.on("serverEventReceived", this._onEventReceived);
  }

  /**
   * Close the EventSourceFront.
   */
  destroy() {
    this.off("serverEventSourceConnectionClosed");
    this.off("serverEventReceived");
    return super.destroy();
  }

  /**
   * The "eventSourceConnectionClosed" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param number httpChannelId
   */
  async _onEventSourceConnectionClosed(httpChannelId) {
    this.emit("eventSourceConnectionClosed", httpChannelId);
  }

  /**
   * The "eventReceived" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param string httpChannelId
   *        Channel ID of the eventSource connection.
   * @param object data
   *        The data received from the server.
   */
  async _onEventReceived(httpChannelId, data) {
    this.emit("eventReceived", httpChannelId, data);
  }
}

exports.EventSourceFront = EventSourceFront;
registerFront(EventSourceFront);
