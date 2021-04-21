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

/**
 * Handles network events from the parent process
 */
class NetworkEventWatcher {
  /**
   * Start watching for all network events related to a given Watcher Actor.
   *
   * @param WatcherActor watcherActor
   *        The watcher actor in the parent process from which we should
   *        observe network events.
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

  get conn() {
    return this.watcherActor.conn;
  }

  get browserId() {
    return this.watcherActor.browserId;
  }

  /**
   * Gets the throttle settings
   *
   * @return {*} data
   *
   */
  getThrottleData() {
    return this.listener.throttleData;
  }

  /**
   * Sets the throttle data
   *
   * @param {*} data
   *
   */
  setThrottleData(data) {
    this.listener.throttleData = data;
  }

  /**
   * Block requests based on the filters
   * @param {Object} filters
   */
  blockRequest(filters) {
    this.listener.blockRequest(filters);
  }

  /**
   * Unblock requests based on the fitlers
   * @param {Object} filters
   */
  unblockRequest(filters) {
    this.listener.unblockRequest(filters);
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

    if (this.networkEvents.has(channelId)) {
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

    this.networkEvents.set(resource.resourceId, {
      resourceId: resource.resourceId,
      resourceType: resource.resourceType,
      isBlocked: !!resource.blockedReason,
      types: [],
      resourceUpdates: {},
    });

    this.onNetworkEventAvailable([resource]);
    return actor;
  }

  onNetworkEventUpdate(updateResource) {
    const networkEvent = this.networkEvents.get(updateResource.resourceId);

    if (!networkEvent) {
      return;
    }

    const {
      resourceId,
      resourceType,
      resourceUpdates,
      types,
      isBlocked,
    } = networkEvent;

    switch (updateResource.updateType) {
      case "responseStart":
        resourceUpdates.httpVersion = updateResource.httpVersion;
        resourceUpdates.status = updateResource.status;
        resourceUpdates.statusText = updateResource.statusText;
        resourceUpdates.remoteAddress = updateResource.remoteAddress;
        resourceUpdates.remotePort = updateResource.remotePort;
        // The mimetype is only set when then the contentType is available
        // in the _onResponseHeader and not for cached/service worker requests
        // in _httpResponseExaminer.
        resourceUpdates.mimeType = updateResource.mimeType;
        resourceUpdates.waitingTime = updateResource.waitingTime;
        break;
      case "responseContent":
        resourceUpdates.contentSize = updateResource.contentSize;
        resourceUpdates.transferredSize = updateResource.transferredSize;
        resourceUpdates.mimeType = updateResource.mimeType;
        resourceUpdates.blockingExtension = updateResource.blockingExtension;
        resourceUpdates.blockedReason = updateResource.blockedReason;
        break;
      case "eventTimings":
        resourceUpdates.totalTime = updateResource.totalTime;
        break;
      case "securityInfo":
        resourceUpdates.securityState = updateResource.state;
        resourceUpdates.isRacing = updateResource.isRacing;
        break;
    }

    resourceUpdates[`${updateResource.updateType}Available`] = true;
    types.push(updateResource.updateType);

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
      !types.includes("responseContent") ||
      !types.includes("securityInfo")
    ) {
      return;
    }

    this.onNetworkEventUpdated([
      {
        resourceType,
        resourceId,
        resourceUpdates,
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
