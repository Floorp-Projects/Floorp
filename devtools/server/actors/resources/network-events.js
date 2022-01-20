/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { Pool } = require("devtools/shared/protocol/Pool");

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
    this.pool = new Pool(watcherActor.conn, "network-events");
    this.watcherActor.manage(this.pool);
    this.onNetworkEventAvailable = onAvailable;
    this.onNetworkEventUpdated = onUpdated;
    // Boolean to know if we keep previous document network events or not.
    this.persist = false;
    this.listener = new NetworkObserver(
      { browserId: this.browserId, addonId: watcherActor.context.addonId },
      { onNetworkEvent: this.onNetworkEvent.bind(this) }
    );

    this.listener.init();
    Services.obs.addObserver(this, "window-global-destroyed");
  }

  get conn() {
    return this.watcherActor.conn;
  }

  get browserId() {
    return this.watcherActor.context.browserId;
  }

  /**
   * Instruct to keep reference to previous document requests or not.
   * If persist is disabled, we will clear all informations about previous document
   * on each navigation.
   * If persist is enabled, we will keep all informations for all documents, leading
   * to lots of allocations!
   *
   * @param {Boolean} enabled
   */
  setPersist(enabled) {
    this.persist = enabled;
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
   * Instruct to save or ignore request and response bodies
   * @param {Boolean} save
   */
  setSaveRequestAndResponseBodies(save) {
    this.listener.saveRequestAndResponseBodies = save;
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

  /**
   * Watch for previous document being unloaded in order to clear
   * all related network events, in case persist is disabled.
   * (which is the default behavior)
   */
  observe(windowGlobal, topic) {
    if (topic !== "window-global-destroyed") {
      return;
    }
    // If we persist, we will keep all requests allocated.
    if (this.persist) {
      return;
    }
    // If the watcher is bound to one browser element (i.e. a tab), ignore
    // windowGlobals related to other browser elements
    if (
      this.watcherActor.context.type == "browser-element" &&
      windowGlobal.browsingContext.browserId != this.browserId
    ) {
      return;
    }
    // Also ignore the initial document as:
    // - it shouldn't spawn/store any request?
    // - it would clear the navigation request too early
    if (windowGlobal.isInitialDocument) {
      return;
    }
    const { innerWindowId } = windowGlobal;

    for (const child of this.pool.poolChildren()) {
      // Destroy all network events matching the destroyed WindowGlobal
      if (!child.isNavigationRequest) {
        if (child.innerWindowId == innerWindowId) {
          child.destroy();
        }
        // Avoid destroying the navigation request, which is flagged with previous document's innerWindowId.
        // When navigating, the WindowGlobal we navigate *from* will be destroyed and notified here.
        // We should explicitly avoid destroying it here.
        // But, we still want to eventually destroy them.
        // So do this when navigating a second time, we will navigate from a distinct WindowGlobal
        // and check that this is the top level window global and not an iframe one.
        // So that we avoid clearing the top navigation when an iframe navigates
      } else if (
        child.innerWindowId != innerWindowId &&
        windowGlobal.browsingContext ==
          this.watcherActor.browserElement.browsingContext
      ) {
        child.destroy();
      }
    }
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
    this.pool.manage(actor);

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
      Services.obs.removeObserver(this, "window-global-destroyed");
      this.pool.destroy();
    }
  }
}

module.exports = NetworkEventWatcher;
