/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const { memorySpec } = require("resource://devtools/shared/specs/memory.js");

const { Memory } = require("resource://devtools/server/performance/memory.js");
const {
  actorBridgeWithSpec,
} = require("resource://devtools/server/actors/common.js");

loader.lazyRequireGetter(
  this,
  "StackFrameCache",
  "resource://devtools/server/actors/utils/stack.js",
  true
);

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
exports.MemoryActor = class MemoryActor extends Actor {
  constructor(conn, parent, frameCache = new StackFrameCache()) {
    super(conn, memorySpec);

    this._onGarbageCollection = this._onGarbageCollection.bind(this);
    this._onAllocations = this._onAllocations.bind(this);
    this.bridge = new Memory(parent, frameCache);
    this.bridge.on("garbage-collection", this._onGarbageCollection);
    this.bridge.on("allocations", this._onAllocations);
  }

  destroy() {
    this.bridge.off("garbage-collection", this._onGarbageCollection);
    this.bridge.off("allocations", this._onAllocations);
    this.bridge.destroy();
    super.destroy();
  }

  attach = actorBridgeWithSpec("attach");

  detach = actorBridgeWithSpec("detach");

  getState = actorBridgeWithSpec("getState");

  saveHeapSnapshot(boundaries) {
    return this.bridge.saveHeapSnapshot(boundaries);
  }

  takeCensus = actorBridgeWithSpec("takeCensus");

  startRecordingAllocations = actorBridgeWithSpec("startRecordingAllocations");

  stopRecordingAllocations = actorBridgeWithSpec("stopRecordingAllocations");

  getAllocationsSettings = actorBridgeWithSpec("getAllocationsSettings");

  getAllocations = actorBridgeWithSpec("getAllocations");

  forceGarbageCollection = actorBridgeWithSpec("forceGarbageCollection");

  forceCycleCollection = actorBridgeWithSpec("forceCycleCollection");

  measure = actorBridgeWithSpec("measure");

  residentUnique = actorBridgeWithSpec("residentUnique");

  _onGarbageCollection(data) {
    if (this.conn.transport) {
      this.emit("garbage-collection", data);
    }
  }

  _onAllocations(data) {
    if (this.conn.transport) {
      this.emit("allocations", data);
    }
  }
};
