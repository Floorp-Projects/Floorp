/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Pool } = require("devtools/shared/protocol/Pool");
const { isWindowGlobalPartOfContext } = ChromeUtils.importESModule(
  "resource://devtools/server/actors/watcher/browsing-context-helpers.sys.mjs"
);
const { WatcherRegistry } = ChromeUtils.importESModule(
  "resource://devtools/server/actors/watcher/WatcherRegistry.sys.mjs"
);
const Targets = require("devtools/server/actors/targets/index");

loader.lazyRequireGetter(
  this,
  "NetworkObserver",
  "devtools/server/actors/network-monitor/network-observer",
  true
);

loader.lazyRequireGetter(
  this,
  "NetworkUtils",
  "devtools/server/actors/network-monitor/utils/network-utils"
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
      { sessionContext: watcherActor.sessionContext },
      {
        onNetworkEvent: this.onNetworkEvent.bind(this),
        shouldIgnoreChannel: this.shouldIgnoreChannel.bind(this),
      }
    );

    this.listener.init();
    Services.obs.addObserver(this, "window-global-destroyed");
  }

  /**
   * Clear all the network events and the related actors.
   */
  clear() {
    this.networkEvents.clear();
    this.listener.clear();
    this.pool.destroy();
  }

  get conn() {
    return this.watcherActor.conn;
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
    // For now, consider that the Browser console and toolbox persist all the requests.
    if (this.persist || this.watcherActor.sessionContext.type == "all") {
      return;
    }
    // Only process WindowGlobals which are related to the debugged scope.
    if (
      !isWindowGlobalPartOfContext(
        windowGlobal,
        this.watcherActor.sessionContext
      )
    ) {
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
        //
        // Avoid destroying the request if innerWindowId isn't set. This happens when we reload many times in a row.
        // The previous navigation request will be cancelled and because of that its innerWindowId will be null.
        // But the frontend will receive it after the navigation begins (after will-navigate) and will display it
        // and try to fetch extra data about it. So, avoid destroying its NetworkEventActor.
      } else if (
        child.innerWindowId &&
        child.innerWindowId != innerWindowId &&
        windowGlobal.browsingContext ==
          this.watcherActor.browserElement?.browsingContext
      ) {
        child.destroy();
      }
    }
  }

  /**
   * Called by NetworkObserver in order to know if the channel should be ignored
   */
  shouldIgnoreChannel(channel) {
    // When we are in the browser toolbox in parent process scope,
    // the session context is still "all", but we are no longer watching frame and process targets.
    // In this case, we should ignore all requests belonging to a BrowsingContext that isn't in the parent process
    // (i.e. the process where this Watcher runs)
    const isParentProcessOnlyBrowserToolbox =
      this.watcherActor.sessionContext.type == "all" &&
      !WatcherRegistry.isWatchingTargets(
        this.watcherActor,
        Targets.TYPES.FRAME
      );
    if (isParentProcessOnlyBrowserToolbox) {
      // We should ignore all requests coming from BrowsingContext running in another process
      const browsingContextID = NetworkUtils.getChannelBrowsingContextID(
        channel
      );
      const browsingContext = BrowsingContext.get(browsingContextID);
      // We accept any request that isn't bound to any BrowsingContext.
      // This is most likely a privileged request done from a JSM/C++.
      // `isInProcess` will be true, when the document executes in the parent process.
      //
      // Note that we will still accept all requests that aren't bound to any BrowsingContext
      // See browser_resources_network_events_parent_process.js test with privileged request
      // made from the content processes.
      // We miss some attribute on channel/loadInfo to know that it comes from the content process.
      if (browsingContext?.currentWindowGlobal.isInProcess === false) {
        return true;
      }
    }
    return false;
  }

  onNetworkEvent(event) {
    const { channelId } = event;

    if (this.networkEvents.has(channelId)) {
      throw new Error(
        `Got notified about channel ${channelId} more than once.`
      );
    }

    const actor = new NetworkEventActor(
      this.watcherActor.conn,
      this.watcherActor.sessionContext,
      {
        onNetworkEventUpdate: this.onNetworkEventUpdate.bind(this),
        onNetworkEventDestroy: this.onNetworkEventDestroy.bind(this),
      },
      event
    );
    this.pool.manage(actor);

    const resource = actor.asResource();

    this.networkEvents.set(resource.resourceId, {
      browsingContextID: resource.browsingContextID,
      innerWindowId: resource.innerWindowId,
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
      browsingContextID,
      innerWindowId,
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
        browsingContextID,
        innerWindowId,
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
   */
  destroy() {
    if (this.listener) {
      this.clear();
      this.listener.destroy();
      Services.obs.removeObserver(this, "window-global-destroyed");
    }
  }
}

module.exports = NetworkEventWatcher;
