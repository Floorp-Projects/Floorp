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

    Services.obs.addObserver(
      this.httpFailedOpeningRequest,
      "http-on-failed-opening-request"
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
  }
}

module.exports = NetworkEventContentWatcher;
