/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

module.exports = async function({ targetFront, onAvailable }) {
  const eventSourceFront = await targetFront.getFront("eventSource");
  eventSourceFront.startListening();

  eventSourceFront.on("eventSourceConnectionClosed", httpChannelId => {
    const resource = createResource("eventSourceConnectionClosed", {
      httpChannelId,
    });
    onAvailable([resource]);
  });

  eventSourceFront.on("eventReceived", (httpChannelId, data) => {
    const resource = createResource("eventReceived", { httpChannelId, data });
    onAvailable([resource]);
  });
};

function createResource(messageType, eventParams) {
  return {
    resourceType: ResourceWatcher.TYPES.SERVER_SENT_EVENT,
    messageType,
    ...eventParams,
  };
}
