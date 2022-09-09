/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const { Ci } = require("chrome");

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
  /**
   * Allows clearing of network events
   */
  clear() {
    this._networkEvents.clear();
  }

  get conn() {
    return this.targetActor.conn;
  }

  httpFailedOpeningRequest(subject, topic) {
    const channel = subject.QueryInterface(Ci.nsIHttpChannel);

    // Ignore preload requests to avoid duplicity request entries in
    // the Network panel. If a preload fails (for whatever reason)
    // then the platform kicks off another 'real' request.
    if (NetworkUtils.isPreloadRequest(channel)) {
      return;
    }

    if (
      !NetworkUtils.matchRequest(channel, {
        targetActor: this.targetActor,
      })
    ) {
      return;
    }

    this.onNetworkEventAvailable(channel, {
      networkEventOptions: {
        blockedReason: channel.loadInfo.requestBlockingReason,
      },
      resourceOverrides: null,
      onNetworkEventUpdate: this.onFailedNetworkEventUpdated.bind(this),
    });
  }

  httpOnImageCacheResponse(subject, topic) {
    if (
      topic != "http-on-image-cache-response" ||
      !(subject instanceof Ci.nsIHttpChannel)
    ) {
      return;
    }

    const channel = subject.QueryInterface(Ci.nsIHttpChannel);

    if (
      !NetworkUtils.matchRequest(channel, {
        targetActor: this.targetActor,
      })
    ) {
      return;
    }

    // Only one network request should be created per URI for images from the cache
    const hasNetworkEventForURI = Array.from(this._networkEvents.values()).find(
      networkEvent => networkEvent.url === channel.URI.spec
    );

    if (hasNetworkEventForURI) {
      return;
    }

    this.onNetworkEventAvailable(channel, {
      networkEventOptions: { fromCache: true },
      resourceOverrides: {
        status: 200,
        statusText: "OK",
        totalTime: 0,
        mimeType: channel.contentType,
        contentSize: channel.contentLength,
      },
      onNetworkEventUpdate: this.onImageCacheNetworkEventUpdated.bind(this),
    });
  }

  onNetworkEventAvailable(
    channel,
    { networkEventOptions, resourceOverrides, onNetworkEventUpdate }
  ) {
    const event = NetworkUtils.createNetworkEvent(channel, networkEventOptions);

    const actor = new NetworkEventActor(
      this.conn,
      this.targetActor.sessionContext,
      {
        onNetworkEventUpdate,
        onNetworkEventDestroy: this.onNetworkEventDestroyed.bind(this),
      },
      event
    );
    this.targetActor.manage(actor);

    const resource = actor.asResource();

    this._networkEvents.set(resource.resourceId, {
      browsingContextID: resource.browsingContextID,
      innerWindowId: resource.innerWindowId,
      resourceId: resource.resourceId,
      resourceType: resource.resourceType,
      url: resource.url,
      types: [],
      resourceUpdates: {},
    });

    // Override the default resource property values if need be
    if (resourceOverrides) {
      for (const prop in resourceOverrides) {
        resource[prop] = resourceOverrides[prop];
      }
    }

    this.onAvailable([resource]);
    NetworkUtils.fetchRequestHeadersAndCookies(channel, actor, {});
  }

  /*
   * When an update is needed for a network event.
   *
   * @param {Object} updateResource
   *                 The resource to be updated
   * @param {Array} allRequiredUpdates
   *                The updates that are essential to be received before notifying
   *                the client. Other updates may or may not be available.
   */

  onNetworkEventUpdated(updateResource, allRequiredUpdates) {
    const networkEvent = this._networkEvents.get(updateResource.resourceId);

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
    } = networkEvent;

    resourceUpdates[`${updateResource.updateType}Available`] = true;
    types.push(updateResource.updateType);

    if (allRequiredUpdates.every(header => types.includes(header))) {
      this.onUpdated([
        {
          browsingContextID,
          innerWindowId,
          resourceType,
          resourceId,
          resourceUpdates,
        },
      ]);
    }
  }

  onFailedNetworkEventUpdated(updateResource) {
    this.onNetworkEventUpdated(updateResource, [
      "requestHeaders",
      "requestCookies",
    ]);
  }

  onImageCacheNetworkEventUpdated(updateResource) {
    this.onNetworkEventUpdated(updateResource, ["requestHeaders"]);
  }

  onNetworkEventDestroyed(channelId) {
    if (this._networkEvents.has(channelId)) {
      this._networkEvents.delete(channelId);
    }
  }

  destroy() {
    this.clear();
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
