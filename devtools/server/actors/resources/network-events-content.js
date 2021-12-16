/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const { Ci } = require("chrome");
const Services = require("Services");

loader.lazyRequireGetter(
  this,
  "NetworkEventActor",
  "devtools/server/actors/network-monitor/network-event-actor",
  true
);

loader.lazyRequireGetter(
  this,
  "NetworkUtils",
  "devtools/server/actors/network-monitor/utils/network-utils"
);

/**
 * Handles network events from the content process
 * This currently only handles events for requests (js/css) blocked by CSP.
 */
class NetworkEventContentWatcher {
  /**
   * Start watching for all network events related to a given Target Actor.
   *
   * @param TargetActor targetActor
   *        The target actor in the content process from which we should
   *        observe network events.
   * @param Object options
   *        Dictionary object with following attributes:
   *        - onAvailable: mandatory function
   *          This will be called for each resource.
   *        - onUpdated: optional function
   *          This would be called multiple times for each resource.
   */
  async watch(targetActor, { onAvailable, onUpdated }) {
    this._networkEvents = new Map();

    this.targetActor = targetActor;
    this.onAvailable = onAvailable;
    this.onUpdated = onUpdated;

    this.httpFailedOpeningRequest = this.httpFailedOpeningRequest.bind(this);
    this.httpOnImageCacheResponse = this.httpOnImageCacheResponse.bind(this);

    Services.obs.addObserver(
      this.httpFailedOpeningRequest,
      "http-on-failed-opening-request"
    );

    Services.obs.addObserver(
      this.httpOnImageCacheResponse,
      "http-on-image-cache-response"
    );
  }

  get conn() {
    return this.targetActor.conn;
  }

  get browserId() {
    return this.targetActor.browserId;
  }

  httpFailedOpeningRequest(subject, topic) {
    const channel = subject.QueryInterface(Ci.nsIHttpChannel);

    // Ignore preload requests to avoid duplicity request entries in
    // the Network panel. If a preload fails (for whatever reason)
    // then the platform kicks off another 'real' request.
    if (NetworkUtils.isPreloadRequest(channel)) {
      return;
    }

    // XXX: is window the best filter to use?
    if (
      !NetworkUtils.matchRequest(channel, {
        window: this.targetActor.window,
        matchExactWindow: this.targetActor.ignoreSubFrames,
      })
    ) {
      return;
    }

    const event = NetworkUtils.createNetworkEvent(channel, {
      blockedReason: channel.loadInfo.requestBlockingReason,
    });

    const actor = new NetworkEventActor(
      this,
      {
        onNetworkEventUpdate: this.onNetworkEventUpdated.bind(this),
        onNetworkEventDestroy: this.onNetworkEventDestroyed.bind(this),
      },
      event
    );
    this.targetActor.manage(actor);

    const resource = actor.asResource();

    this._networkEvents.set(resource.resourceId, {
      resourceId: resource.resourceId,
      resourceType: resource.resourceType,
      types: [],
      resourceUpdates: {},
    });

    this.onAvailable([resource]);
    NetworkUtils.fetchRequestHeadersAndCookies(channel, actor, {});
  }

  httpOnImageCacheResponse(subject, topic) {
    if (
      topic != "http-on-image-cache-response" ||
      !(subject instanceof Ci.nsIHttpChannel)
    ) {
      return;
    }

    const channel = subject.QueryInterface(Ci.nsIHttpChannel);

    // XXX: is window the best filter to use?
    if (
      !NetworkUtils.matchRequest(channel, {
        window: this.targetActor.window,
        matchExactWindow: this.targetActor.ignoreSubFrames,
      })
    ) {
      return;
    }

    const event = NetworkUtils.createNetworkEvent(channel, {
      fromCache: true,
    });

    const actor = new NetworkEventActor(
      this,
      {
        onNetworkEventUpdate: this.onNetworkEventUpdatedForImageCache.bind(
          this
        ),
        onNetworkEventDestroy: this.onNetworkEventDestroyed.bind(this),
      },
      event
    );
    this.targetActor.manage(actor);

    const resource = actor.asResource();

    this._networkEvents.set(resource.resourceId, {
      resourceId: resource.resourceId,
      resourceType: resource.resourceType,
      types: [],
      resourceUpdates: {},
    });

    // The channel we get here is a dummy channel and no real internet
    // connection has been made, thus some dummy values need to be
    // set.
    resource.status = 200;
    resource.statusText = "OK";
    resource.totalTime = 0;
    resource.mimeType = channel.contentType;
    resource.contentSize = channel.contentLength;

    this.onAvailable([resource]);
    NetworkUtils.fetchRequestHeadersAndCookies(channel, actor, {});
  }

  onNetworkEventUpdatedForImageCache(updateResource) {
    const networkEvent = this._networkEvents.get(updateResource.resourceId);

    if (!networkEvent) {
      return;
    }

    const { resourceId, resourceType, resourceUpdates, types } = networkEvent;

    resourceUpdates[`${updateResource.updateType}Available`] = true;
    types.push(updateResource.updateType);

    if (types.includes("requestHeaders")) {
      this.onUpdated([{ resourceType, resourceId, resourceUpdates }]);
    }
  }

  onNetworkEventUpdated(updateResource) {
    const networkEvent = this._networkEvents.get(updateResource.resourceId);

    if (!networkEvent) {
      return;
    }

    const { resourceId, resourceType, resourceUpdates, types } = networkEvent;

    resourceUpdates[`${updateResource.updateType}Available`] = true;
    types.push(updateResource.updateType);

    if (types.includes("requestHeaders") && types.includes("requestCookies")) {
      this.onUpdated([{ resourceType, resourceId, resourceUpdates }]);
    }
  }

  onNetworkEventDestroyed(channelId) {
    if (this._networkEvents.has(channelId)) {
      this._networkEvents.delete(channelId);
    }
  }

  destroy() {
    Services.obs.removeObserver(
      this.httpFailedOpeningRequest,
      "http-on-failed-opening-request"
    );

    Services.obs.removeObserver(
      this.httpOnImageCacheResponse,
      "http-on-image-cache-response"
    );
  }
}

module.exports = NetworkEventContentWatcher;
