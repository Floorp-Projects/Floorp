/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

module.exports = async function({ targetCommand, targetFront, onAvailable }) {
  function onNetworkEventStackTrace(packet) {
    const actor = packet.eventActor;
    onAvailable([
      {
        resourceType: ResourceWatcher.TYPES.NETWORK_EVENT_STACKTRACE,
        resourceId: actor.channelId,
        stacktraceAvailable: actor.cause.stacktraceAvailable,
        lastFrame: actor.cause.lastFrame,
      },
    ]);
  }
  const webConsoleFront = await targetFront.getFront("console");
  webConsoleFront.on("serverNetworkStackTrace", onNetworkEventStackTrace);
};
