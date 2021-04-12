/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

module.exports = async function({
  targetCommand,
  targetFront,
  onAvailable,
  onUpdated,
}) {
  // Allow the top level target unconditionnally.
  // Also allow frame, but only in content toolbox, i.e. still ignore them in
  // the context of the browser toolbox as we inspect messages via the process
  // targets
  // Also ignore workers as they are not supported yet. (see bug 1592584)
  const listenForFrames = targetCommand.descriptorFront.isLocalTab;
  const isAllowed =
    targetFront.isTopLevel ||
    targetFront.targetType === targetCommand.TYPES.PROCESS ||
    (targetFront.targetType === targetCommand.TYPES.FRAME && listenForFrames);

  if (!isAllowed) {
    return;
  }

  const webConsoleFront = await targetFront.getFront("console");
  const resources = new Map();

  function onNetworkEvent(packet) {
    const actor = packet.eventActor;

    resources.set(actor.actor, {
      resourceId: actor.channelId,
      resourceType: ResourceWatcher.TYPES.NETWORK_EVENT,
      isBlocked: !!actor.blockedReason,
      types: [],
      resourceUpdates: {},
    });

    onAvailable([
      {
        resourceId: actor.channelId,
        resourceType: ResourceWatcher.TYPES.NETWORK_EVENT,
        timeStamp: actor.timeStamp,
        actor: actor.actor,
        startedDateTime: actor.startedDateTime,
        url: actor.url,
        method: actor.method,
        isXHR: actor.isXHR,
        cause: {
          type: actor.cause.type,
          loadingDocumentUri: actor.cause.loadingDocumentUri,
        },
        timings: {},
        private: actor.private,
        fromCache: actor.fromCache,
        fromServiceWorker: actor.fromServiceWorker,
        isThirdPartyTrackingResource: actor.isThirdPartyTrackingResource,
        referrerPolicy: actor.referrerPolicy,
        blockedReason: actor.blockedReason,
        blockingExtension: actor.blockingExtension,
        stacktraceResourceId:
          actor.cause.type == "websocket"
            ? actor.url.replace(/^http/, "ws")
            : actor.channelId,
      },
    ]);
  }

  function onNetworkEventUpdate(packet) {
    const resource = resources.get(packet.from);

    if (!resource) {
      return;
    }

    const {
      types,
      resourceUpdates,
      resourceId,
      resourceType,
      isBlocked,
    } = resource;

    switch (packet.updateType) {
      case "responseStart":
        resourceUpdates.httpVersion = packet.response.httpVersion;
        resourceUpdates.status = packet.response.status;
        resourceUpdates.statusText = packet.response.statusText;
        resourceUpdates.remoteAddress = packet.response.remoteAddress;
        resourceUpdates.remotePort = packet.response.remotePort;
        resourceUpdates.mimeType = packet.response.mimeType;
        resourceUpdates.waitingTime = packet.response.waitingTime;
        break;
      case "responseContent":
        resourceUpdates.contentSize = packet.contentSize;
        resourceUpdates.transferredSize = packet.transferredSize;
        resourceUpdates.mimeType = packet.mimeType;
        resourceUpdates.blockingExtension = packet.blockingExtension;
        resourceUpdates.blockedReason = packet.blockedReason;
        break;
      case "eventTimings":
        resourceUpdates.totalTime = packet.totalTime;
        break;
      case "securityInfo":
        resourceUpdates.securityState = packet.state;
        resourceUpdates.isRacing = packet.isRacing;
        break;
    }

    resourceUpdates[`${packet.updateType}Available`] = true;
    types.push(packet.updateType);

    if (isBlocked) {
      // Blocked requests
      if (
        !types.includes("requestHeaders") ||
        !types.includes("requestCookies")
      ) {
        return;
      }
    } else if (
      // Un-blocked requests
      !types.includes("requestHeaders") ||
      !types.includes("requestCookies") ||
      !types.includes("eventTimings") ||
      !types.includes("responseContent")
    ) {
      return;
    }

    onUpdated([
      {
        resourceType,
        resourceId,
        resourceUpdates,
      },
    ]);
  }

  webConsoleFront.on("serverNetworkEvent", onNetworkEvent);
  webConsoleFront.on("serverNetworkUpdateEvent", onNetworkEventUpdate);
  // Start listening to network events
  await webConsoleFront.startListeners(["NetworkActivity"]);
};
