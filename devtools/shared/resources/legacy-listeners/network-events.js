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
      timeStamp: actor.timeStamp,
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
      stacktraceResourceId:
        actor.cause.type == "websocket"
          ? actor.url.replace(/^http/, "ws")
          : actor.channelId,
      updates: [],
    };

    // Lets remove the stacktrace info here as
    // it is passed from the the server by the NETWORK_EVENT_STACKTRACE
    // resource type.
    delete resource.cause.stacktraceAvailable;
    delete resource.cause.lastFrame;

    _resources.set(actor.actor, resource);
    onAvailable([resource]);
  }

  function onNetworkEventUpdate(packet) {
    const resource = _resources.get(packet.from);

    if (!resource) {
      return;
    }

    const updateType = packet.updateType;
    const resourceUpdates = {};
    resourceUpdates.updates = [...resource.updates, updateType];

    switch (updateType) {
      case "requestHeaders":
        resourceUpdates.request = Object.assign({}, resource.request, {
          headersSize: packet.headersSize,
        });
        break;
      case "requestPostData":
        resourceUpdates.discardRequestBody = packet.discardRequestBody;
        resourceUpdates.request = Object.assign({}, resource.request, {
          bodySize: packet.dataSize,
        });
        break;
      case "responseStart":
        resourceUpdates.response = Object.assign({}, resource.response, {
          httpVersion: packet.response.httpVersion,
          status: packet.response.status,
          statusText: packet.response.statusText,
          headersSize: packet.response.headersSize,
          remoteAddress: packet.response.remoteAddress,
          remotePort: packet.response.remotePort,
          content: {
            mimeType: packet.response.mimeType,
          },
          waitingTime: packet.response.waitingTime,
        });
        resourceUpdates.discardResponseBody =
          packet.response.discardResponseBody;
        break;
      case "responseContent":
        resourceUpdates.discardResponseBody = packet.discardResponseBody;
        resourceUpdates.response = Object.assign({}, resource.response, {
          bodySize: packet.contentSize,
          transferredSize: packet.transferredSize,
          content: { mimeType: packet.mimeType },
        });
        break;
      case "eventTimings":
        resourceUpdates.totalTime = packet.totalTime;
        break;
      case "securityInfo":
        resourceUpdates.securityState = packet.state;
        resourceUpdates.isRacing = packet.isRacing;
        break;
      case "responseCache":
        resourceUpdates.response = Object.assign({}, resource.response, {
          responseCache: packet.responseCache,
        });
        break;
    }

    // Update local resource.
    Object.assign(resource, resourceUpdates);

    onUpdated([
      {
        resourceType: resource.resourceType,
        resourceId: resource.resourceId,
        resourceUpdates,
        updateType,
      },
    ]);

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
