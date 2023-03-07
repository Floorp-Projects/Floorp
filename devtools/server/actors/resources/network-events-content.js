/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(
  this,
  "NetworkEventActor",
  "resource://devtools/server/actors/network-monitor/network-event-actor.js",
  true
);

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  NetworkUtils:
    "resource://devtools/shared/network-observer/NetworkUtils.sys.mjs",
});

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
    // Map from channelId to URI strings.
    // Used to check if we already emitted an event for a cached image, as only
    // one event should be emitted per URI.
    this._imageCacheURIs = new Map();

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
    this._imageCacheURIs.clear();
  }

  httpFailedOpeningRequest(subject, topic) {
    const channel = subject.QueryInterface(Ci.nsIHttpChannel);

    // Ignore preload requests to avoid duplicity request entries in
    // the Network panel. If a preload fails (for whatever reason)
    // then the platform kicks off another 'real' request.
    if (lazy.NetworkUtils.isPreloadRequest(channel)) {
      return;
    }

    if (
      !lazy.NetworkUtils.matchRequest(channel, {
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
      !lazy.NetworkUtils.matchRequest(channel, {
        targetActor: this.targetActor,
      })
    ) {
      return;
    }

    // Only one network request should be created per URI for images from the cache
    const hasURI = Array.from(this._imageCacheURIs.values()).some(
      uri => uri === channel.URI.spec
    );

    if (hasURI) {
      return;
    }

    this._imageCacheURIs.set(channel.channelId, channel.URI.spec);

    this.onNetworkEventAvailable(channel, {
      networkEventOptions: { fromCache: true },
      resourceOverrides: {
        status: 200,
        statusText: "OK",
        totalTime: 0,
        mimeType: channel.contentType,
        contentSize: channel.contentLength,
      },
    });
  }

  onNetworkEventAvailable(channel, { networkEventOptions, resourceOverrides }) {
    const actor = new NetworkEventActor(
      this.targetActor.conn,
      this.targetActor.sessionContext,
      {
        onNetworkEventDestroy: this.onNetworkEventDestroyed.bind(this),
      },
      networkEventOptions,
      channel
    );
    this.targetActor.manage(actor);

    const resource = actor.asResource();

    // Override the default resource property values if need be
    if (resourceOverrides) {
      for (const prop in resourceOverrides) {
        resource[prop] = resourceOverrides[prop];
      }
    }

    this.onAvailable([resource]);

    this.onUpdated([
      {
        browsingContextID: resource.browsingContextID,
        innerWindowId: resource.innerWindowId,
        resourceType: resource.resourceType,
        resourceId: resource.resourceId,
        resourceUpdates: {
          requestCookiesAvailable: true,
          requestHeadersAvailable: true,
        },
      },
    ]);
  }

  onNetworkEventDestroyed(channelId) {
    if (this._imageCacheURIs.has(channelId)) {
      this._imageCacheURIs.delete(channelId);
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
