/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ActorClassWithSpec, Actor } = require("devtools/shared/protocol");
const { networkParentSpec } = require("devtools/shared/specs/network-parent");

const {
  TYPES: { NETWORK_EVENT },
  getResourceWatcher,
} = require("devtools/server/actors/resources/index");

/**
 * This actor manages all network functionality running
 * in the parent process.
 *
 * @constructor
 *
 */
const NetworkParentActor = ActorClassWithSpec(networkParentSpec, {
  initialize(watcherActor) {
    this.watcherActor = watcherActor;
    Actor.prototype.initialize.call(this, this.watcherActor.conn);
  },

  // Caches the throttling data so that on clearing the
  // current network throttling it can be reset to the previous.
  defaultThrottleData: undefined,

  isEqual(next, current) {
    // If both objects, check all entries
    if (current && next && next == current) {
      return Object.entries(current).every(([k, v]) => {
        return next[k] === v;
      });
    }
    return false;
  },

  destroy(conn) {
    Actor.prototype.destroy.call(this, conn);
  },

  get networkEventWatcher() {
    return getResourceWatcher(this.watcherActor, NETWORK_EVENT);
  },

  setNetworkThrottling(throttleData) {
    if (!this.networkEventWatcher) {
      throw new Error("Not listening for network events");
    }

    if (throttleData !== null) {
      throttleData = {
        latencyMean: throttleData.latency,
        latencyMax: throttleData.latency,
        downloadBPSMean: throttleData.downloadThroughput,
        downloadBPSMax: throttleData.downloadThroughput,
        uploadBPSMean: throttleData.uploadThroughput,
        uploadBPSMax: throttleData.uploadThroughput,
      };
    }

    const currentThrottleData = this.networkEventWatcher.getThrottleData();
    if (this.isEqual(throttleData, currentThrottleData)) {
      return;
    }

    if (this.defaultThrottleData === undefined) {
      this.defaultThrottleData = currentThrottleData;
    }

    this.networkEventWatcher.setThrottleData(throttleData);
  },

  getNetworkThrottling() {
    if (!this.networkEventWatcher) {
      throw new Error("Not listening for network events");
    }
    const throttleData = this.networkEventWatcher.getThrottleData();
    if (!throttleData) {
      return null;
    }
    return {
      downloadThroughput: throttleData.downloadBPSMax,
      uploadThroughput: throttleData.uploadBPSMax,
      latency: throttleData.latencyMax,
    };
  },

  clearNetworkThrottling() {
    if (this.defaultThrottleData !== undefined) {
      this.setNetworkThrottling(this.defaultThrottleData);
    }
  },

  /**
   * Sets the urls to block.
   *
   * @param Array urls
   *         The response packet - stack trace.
   */
  setBlockedUrls(urls) {
    if (!this.networkEventWatcher) {
      throw new Error("Not listening for network events");
    }
    this.networkEventWatcher.setBlockedUrls(urls);
    return {};
  },

  /**
   * Returns the urls that are block
   */
  getBlockedUrls() {
    if (!this.networkEventWatcher) {
      throw new Error("Not listening for network events");
    }
    return this.networkEventWatcher.getBlockedUrls();
  },

  /**
   * Blocks the requests based on the filters
   * @param {Object} filters
   */
  blockRequest(filters) {
    if (!this.networkEventWatcher) {
      throw new Error("Not listening for network events");
    }
    this.networkEventWatcher.blockRequest(filters);
  },

  /**
   * Unblocks requests based on the filters
   * @param {Object} filters
   */
  unblockRequest(filters) {
    if (!this.networkEventWatcher) {
      throw new Error("Not listening for network events");
    }
    this.networkEventWatcher.unblockRequest(filters);
  },
});

exports.NetworkParentActor = NetworkParentActor;
