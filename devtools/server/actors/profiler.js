/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const { Profiler } = require("devtools/server/performance/profiler");
const { actorBridgeWithSpec } = require("devtools/server/actors/common");
const { profilerSpec } = require("devtools/shared/specs/profiler");

/**
 * This actor wraps the Profiler module at devtools/server/performance/profiler.js
 * and provides RDP definitions.
 *
 * @see devtools/server/performance/profiler.js for documentation.
 */
exports.ProfilerActor = ActorClassWithSpec(profilerSpec, {
  initialize: function (conn) {
    Actor.prototype.initialize.call(this, conn);
    this._onProfilerEvent = this._onProfilerEvent.bind(this);

    this.bridge = new Profiler();
    this.bridge.on("*", this._onProfilerEvent);
  },

  destroy: function () {
    this.bridge.off("*", this._onProfilerEvent);
    this.bridge.destroy();
    Actor.prototype.destroy.call(this);
  },

  startProfiler: actorBridgeWithSpec("start"),
  stopProfiler: actorBridgeWithSpec("stop"),
  getProfile: actorBridgeWithSpec("getProfile"),
  getFeatures: actorBridgeWithSpec("getFeatures"),
  getBufferInfo: actorBridgeWithSpec("getBufferInfo"),
  getStartOptions: actorBridgeWithSpec("getStartOptions"),
  isActive: actorBridgeWithSpec("isActive"),
  sharedLibraries: actorBridgeWithSpec("sharedLibraries"),
  registerEventNotifications: actorBridgeWithSpec("registerEventNotifications"),
  unregisterEventNotifications: actorBridgeWithSpec("unregisterEventNotifications"),
  setProfilerStatusInterval: actorBridgeWithSpec("setProfilerStatusInterval"),

  /**
   * Pipe events from Profiler module to this actor.
   */
  _onProfilerEvent: function (eventName, ...data) {
    this.emit(eventName, ...data);
  },
});
