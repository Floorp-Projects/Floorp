/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ActorClassWithSpec, Actor } = require("devtools/shared/protocol");
const { networkSpec } = require("devtools/shared/specs/network");

const {
  TYPES: { NETWORK_EVENT },
  getResourceWatcher,
} = require("devtools/server/actors/resources/index");

/**
 * This actor implements capabilities for handling and supporting
 * Network requests.
 *
 * @constructor
 *
 */
const NetworkActor = ActorClassWithSpec(networkSpec, {
  initialize(watcherActor) {
    this.watcherActor = watcherActor;
    Actor.prototype.initialize.call(this, this.watcherActor.conn);
  },

  destroy(conn) {
    Actor.prototype.destroy.call(this, conn);
  },

  get networkEventWatcher() {
    return getResourceWatcher(this.watcherActor, NETWORK_EVENT);
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
});

exports.NetworkActor = NetworkActor;
