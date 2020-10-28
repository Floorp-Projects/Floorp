/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(
  this,
  "NetworkObserver",
  "devtools/server/actors/network-monitor/network-observer",
  true
);

loader.lazyRequireGetter(
  this,
  "NetworkEventActor",
  "devtools/server/actors/network-monitor/network-event-actor",
  true
);

class NetworkEventWatcher {
  /**
   * Start watching for all network events related to a given Watcher Actor.
   *
   * @param WatcherActor watcherActor
   *        The watcher actor from which we should observe network events
   * @param Object options
   *        Dictionary object with following attributes:
   *        - onAvailable: mandatory function
   *          This will be called for each resource.
   *        - onUpdated: optional function
   *          This would be called multiple times for each resource.
   */
  async watch(watcherActor, { onAvailable, onUpdated }) {
    this.networkEvents = new Map();
    this.watcherActor = watcherActor;
    this.onNetworkEventAvailable = onAvailable;
    this.onNetworkEventUpdated = onUpdated;

    this.listener = new NetworkObserver(
      { browserId: watcherActor.browserId },
      { onNetworkEvent: this.onNetworkEvent.bind(this) }
    );
    this.listener.init();
  }

  /**
   * Calls the listener to set blocked urls
   *
   * @param {Array} urls
   *        The urls to block
   */

  setBlockedUrls(urls) {
    this.listener.setBlockedUrls(urls);
  }

  /**
   * Calls the listener to get the blocked urls
   *
   * @return {Array} urls
   *          The blocked urls
   */

  getBlockedUrls() {
    return this.listener.getBlockedUrls();
  }

  onNetworkEvent(event) {
    const { channelId } = event;
    if (this.networkEvents.get(channelId)) {
      throw new Error(
        `Got notified about channel ${channelId} more than once.`
      );
    }
    const actor = new NetworkEventActor(
      this,
      {
        onNetworkEventUpdate: this.onNetworkEventUpdate.bind(this),
        onNetworkEventDestroy: this.onNetworkEventDestroy.bind(this),
      },
      event
    );
    this.watcherActor.manage(actor);

    const resource = actor.asResource();
    this.networkEvents.set(resource.resourceId, resource);
    this.onNetworkEventAvailable([resource]);
    return actor;
  }

  onNetworkEventUpdate(updateResource) {
    const currentResource = this.networkEvents.get(updateResource.resourceId);
    if (!currentResource) {
      return;
    }
    const resourceUpdates = {
      updates: [...currentResource.updates, updateResource.updateType],
    };

    switch (updateResource.updateType) {
      case "requestHeaders":
        resourceUpdates.request = Object.assign(currentResource.request, {
          headersSize: updateResource.headersSize,
        });
        break;
      case "requestPostData":
        resourceUpdates.discardRequestBody = updateResource.discardRequestBody;
        resourceUpdates.request = Object.assign(currentResource.request, {
          bodySize: updateResource.dataSize,
        });
        break;
      case "responseStart":
        resourceUpdates.response = Object.assign(currentResource.response, {
          httpVersion: updateResource.response.httpVersion,
          status: updateResource.response.status,
          statusText: updateResource.response.statusText,
          headersSize: updateResource.response.headersSize,
          remoteAddress: updateResource.response.remoteAddress,
          remotePort: updateResource.response.remotePort,
          content: {
            mimeType: updateResource.response.mimeType,
          },
          waitingTime: updateResource.response.waitingTime,
        });
        resourceUpdates.discardResponseBody =
          updateResource.response.discardResponseBody;
        break;
      case "responseContent":
        resourceUpdates.response = Object.assign(currentResource.response, {
          content: { mimeType: updateResource.mimeType },
          bodySize: updateResource.contentSize,
          transferredSize: updateResource.transferredSize,
        });
        resourceUpdates.discardResponseBody =
          updateResource.discardResponseBody;
        break;
      case "eventTimings":
        resourceUpdates.totalTime = updateResource.totalTime;
        break;
      case "securityInfo":
        resourceUpdates.securityState = updateResource.state;
        break;
      case "responseCache":
        resourceUpdates.response = Object.assign(currentResource.response, {
          responseCache: updateResource.responseCache,
        });
        break;
    }

    Object.assign(currentResource, resourceUpdates);

    this.onNetworkEventUpdated([
      {
        resourceType: currentResource.resourceType,
        resourceId: updateResource.resourceId,
        resourceUpdates,
        updateType: updateResource.updateType,
      },
    ]);
  }

  onNetworkEventDestroy(channelId) {
    if (this.networkEvents.has(channelId)) {
      this.networkEvents.delete(channelId);
    }
  }

  /**
   * Stop watching for network event related to a given Watcher Actor.
   *
   * @param WatcherActor watcherActor
   *        The watcher actor from which we should stop observing network events
   */
  destroy(watcherActor) {
    if (this.listener) {
      this.listener.destroy();
    }
  }
}

module.exports = NetworkEventWatcher;
