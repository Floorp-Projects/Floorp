/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/shared/protocol");
const { Memory } = require("devtools/server/performance/memory");
const { actorBridgeWithSpec } = require("devtools/server/actors/common");
const { memorySpec } = require("devtools/shared/specs/memory");
loader.lazyRequireGetter(this, "StackFrameCache",
                         "devtools/server/actors/utils/stack", true);

/**
 * An actor that returns memory usage data for its parent actor's window.
 * A target-scoped instance of this actor will measure the memory footprint of
 * the target, such as a tab. A global-scoped instance however, will measure the memory
 * footprint of the chrome window referenced by the root actor.
 *
 * This actor wraps the Memory module at devtools/server/performance/memory.js
 * and provides RDP definitions.
 *
 * @see devtools/server/performance/memory.js for documentation.
 */
exports.MemoryActor = protocol.ActorClassWithSpec(memorySpec, {
  initialize: function(conn, parent, frameCache = new StackFrameCache()) {
    protocol.Actor.prototype.initialize.call(this, conn);

    this._onGarbageCollection = this._onGarbageCollection.bind(this);
    this._onAllocations = this._onAllocations.bind(this);
    this.bridge = new Memory(parent, frameCache);
    this.bridge.on("garbage-collection", this._onGarbageCollection);
    this.bridge.on("allocations", this._onAllocations);
  },

  destroy: function() {
    this.bridge.off("garbage-collection", this._onGarbageCollection);
    this.bridge.off("allocations", this._onAllocations);
    this.bridge.destroy();
    protocol.Actor.prototype.destroy.call(this);
  },

  attach: actorBridgeWithSpec("attach"),

  detach: actorBridgeWithSpec("detach"),

  getState: actorBridgeWithSpec("getState"),

  saveHeapSnapshot: function(boundaries) {
    return this.bridge.saveHeapSnapshot(boundaries);
  },

  takeCensus: actorBridgeWithSpec("takeCensus"),

  startRecordingAllocations: actorBridgeWithSpec("startRecordingAllocations"),

  stopRecordingAllocations: actorBridgeWithSpec("stopRecordingAllocations"),

  getAllocationsSettings: actorBridgeWithSpec("getAllocationsSettings"),

  getAllocations: actorBridgeWithSpec("getAllocations"),

  forceGarbageCollection: actorBridgeWithSpec("forceGarbageCollection"),

  forceCycleCollection: actorBridgeWithSpec("forceCycleCollection"),

  measure: actorBridgeWithSpec("measure"),

  residentUnique: actorBridgeWithSpec("residentUnique"),

  _onGarbageCollection: function(data) {
    if (this.conn.transport) {
      this.emit("garbage-collection", data);
    }
  },

  _onAllocations: function(data) {
    if (this.conn.transport) {
      this.emit("allocations", data);
    }
  },
});
