/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

module.exports = async function({ targetFront, onAvailable }) {
  if (!targetFront.hasActor("webSocket")) {
    return;
  }

  const webSocketFront = await targetFront.getFront("webSocket");
  webSocketFront.startListening();

  webSocketFront.on(
    "webSocketOpened",
    (httpChannelId, effectiveURI, protocols, extensions) => {
      const resource = toResource("webSocketOpened", {
        httpChannelId,
        effectiveURI,
        protocols,
        extensions,
      });
      onAvailable([resource]);
    }
  );

  webSocketFront.on(
    "webSocketClosed",
    (httpChannelId, wasClean, code, reason) => {
      const resource = toResource("webSocketClosed", {
        httpChannelId,
        wasClean,
        code,
        reason,
      });
      onAvailable([resource]);
    }
  );

  webSocketFront.on("frameReceived", (httpChannelId, data) => {
    const resource = toResource("frameReceived", { httpChannelId, data });
    onAvailable([resource]);
  });

  webSocketFront.on("frameSent", (httpChannelId, data) => {
    const resource = toResource("frameSent", { httpChannelId, data });
    onAvailable([resource]);
  });
};

function toResource(wsMessageType, eventParams) {
  return {
    resourceType: ResourceWatcher.TYPES.WEBSOCKET,
    wsMessageType,
    ...eventParams,
  };
}
