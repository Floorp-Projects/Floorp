/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

module.exports = async function({
  targetList,
  targetFront,
  isFissionEnabledOnContentToolbox,
  onAvailable,
  onUpdated,
}) {
  // Allow the top level target unconditionnally.
  // Also allow frame, but only in content toolbox, when the fission/content toolbox pref is
  // set. i.e. still ignore them in the content of the browser toolbox as we inspect
  // messages via the process targets
  // Also ignore workers as they are not supported yet. (see bug 1592584)
  const isContentToolbox = targetList.targetFront.isLocalTab;
  const listenForFrames = isContentToolbox && isFissionEnabledOnContentToolbox;
  const isAllowed =
    targetFront.isTopLevel ||
    targetFront.targetType === targetList.TYPES.PROCESS ||
    (targetFront.targetType === targetList.TYPES.FRAME && listenForFrames);

  if (!isAllowed) {
    return;
  }

  const webConsoleFront = await targetFront.getFront("console");
  const _resources = new Map();

  function onNetworkEvent(packet) {
    const actor = packet.eventActor;
    const resource = {
      resourceId: actor.channelId,
      resourceType: ResourceWatcher.TYPES.NETWORK_EVENT,
      _type: "NetworkEvent",
      timeStamp: actor.timeStamp,
      node: null,
      actor: actor.actor,
      discardRequestBody: true,
      discardResponseBody: true,
      startedDateTime: actor.startedDateTime,
      request: {
        url: actor.url,
        method: actor.method,
      },
      isXHR: actor.isXHR,
      cause: actor.cause,
      response: {},
      timings: {},
      private: actor.private,
      fromCache: actor.fromCache,
      fromServiceWorker: actor.fromServiceWorker,
      isThirdPartyTrackingResource: actor.isThirdPartyTrackingResource,
      referrerPolicy: actor.referrerPolicy,
      blockedReason: actor.blockedReason,
      blockingExtension: actor.blockingExtension,
      channelId: actor.channelId,
      updates: [],
    };
    _resources.set(actor.actor, resource);
    onAvailable([resource]);
  }

  function onNetworkEventUpdate(packet) {
    const resource = _resources.get(packet.from);

    if (!resource) {
      return;
    }

    resource.updates.push(packet.updateType);
    resource.updateType = packet.updateType;

    switch (packet.updateType) {
      case "requestHeaders":
        resource.request.headersSize = packet.headersSize;
        break;
      case "requestPostData":
        resource.discardRequestBody = packet.discardRequestBody;
        resource.request.bodySize = packet.dataSize;
        break;
      case "responseStart":
        resource.response.httpVersion = packet.response.httpVersion;
        resource.response.status = packet.response.status;
        resource.response.statusText = packet.response.statusText;
        resource.response.headersSize = packet.response.headersSize;
        resource.response.remoteAddress = packet.response.remoteAddress;
        resource.response.remotePort = packet.response.remotePort;
        resource.discardResponseBody = packet.response.discardResponseBody;
        resource.response.content = {
          mimeType: packet.response.mimeType,
        };
        resource.response.waitingTime = packet.response.waitingTime;
        break;
      case "responseContent":
        resource.response.content = {
          mimeType: packet.mimeType,
        };
        resource.response.bodySize = packet.contentSize;
        resource.response.transferredSize = packet.transferredSize;
        resource.discardResponseBody = packet.discardResponseBody;
        break;
      case "eventTimings":
        resource.totalTime = packet.totalTime;
        break;
      case "securityInfo":
        resource.securityState = packet.state;
        break;
      case "responseCache":
        resource.response.responseCache = packet.responseCache;
        break;
    }

    onUpdated([resource]);

    if (resource.blockedReason) {
      // Blocked requests
      if (
        resource.updates.includes("requestHeaders") &&
        resource.updates.includes("requestCookies")
      ) {
        _resources.delete(resource.actor);
      }
    } else if (
      resource.updates.includes("requestHeaders") &&
      resource.updates.includes("requestCookies") &&
      resource.updates.includes("eventTimings") &&
      resource.updates.includes("responseContent")
    ) {
      _resources.delete(resource.actor);
    }
  }

  webConsoleFront.on("serverNetworkEvent", onNetworkEvent);
  webConsoleFront.on("serverNetworkUpdateEvent", onNetworkEventUpdate);
  // Start listening to network events
  await webConsoleFront.startListeners(["NetworkActivity"]);
};
