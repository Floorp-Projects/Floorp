/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const protocol = require("devtools/shared/protocol");
const { ActorClassWithSpec, Actor } = protocol;
const { actorBridgeWithSpec } = require("devtools/server/actors/common");
const { perfSpec } = require("devtools/shared/specs/perf");
const {
  ActorReadyGeckoProfilerInterface,
} = require("devtools/server/performance-new/gecko-profiler-interface");

/**
 * Pass on the events from the bridge to the actor.
 * @param {Object} actor The perf actor
 * @param {Array<string>} names The event names
 */
function _bridgeEvents(actor, names) {
  for (const name of names) {
    actor.bridge.on(name, (...args) => actor.emit(name, ...args));
  }
}

/**
 * The PerfActor wraps the Gecko Profiler interface
 */
exports.PerfActor = ActorClassWithSpec(perfSpec, {
  initialize: function(conn, targetActor) {
    Actor.prototype.initialize.call(this, conn);
    // The "bridge" is the actual implementation of the actor. It is abstracted
    // out into its own class so that it can be re-used with the profiler popup.
    this.bridge = new ActorReadyGeckoProfilerInterface({
      // Do not use the gzipped API from the Profiler to capture profiles.
      gzipped: false,
    });

    _bridgeEvents(this, [
      "profile-locked-by-private-browsing",
      "profile-unlocked-from-private-browsing",
      "profiler-started",
      "profiler-stopped",
    ]);
  },

  destroy: function(conn) {
    Actor.prototype.destroy.call(this, conn);
    this.bridge.destroy();
  },

  // Connect the rest of the ActorReadyGeckoProfilerInterface's methods to the PerfActor.
  startProfiler: actorBridgeWithSpec("startProfiler"),
  stopProfilerAndDiscardProfile: actorBridgeWithSpec(
    "stopProfilerAndDiscardProfile"
  ),
  getSymbolTable: actorBridgeWithSpec("getSymbolTable"),
  getProfileAndStopProfiler: actorBridgeWithSpec("getProfileAndStopProfiler"),
  isActive: actorBridgeWithSpec("isActive"),
  isSupportedPlatform: actorBridgeWithSpec("isSupportedPlatform"),
  isLockedForPrivateBrowsing: actorBridgeWithSpec("isLockedForPrivateBrowsing"),
});
