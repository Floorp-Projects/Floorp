/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu, components } = require("chrome");
const protocol = require("devtools/shared/protocol");
const { method, RetVal, Arg, types } = protocol;
const { Memory } = require("devtools/server/performance/memory");
const { actorBridge } = require("devtools/server/actors/common");
const { Task } = require("devtools/shared/task");
loader.lazyRequireGetter(this, "events", "sdk/event/core");
loader.lazyRequireGetter(this, "StackFrameCache",
                         "devtools/server/actors/utils/stack", true);
loader.lazyRequireGetter(this, "FileUtils",
                         "resource://gre/modules/FileUtils.jsm", true);
loader.lazyRequireGetter(this, "NetUtil", "resource://gre/modules/NetUtil.jsm", true);
loader.lazyRequireGetter(this, "HeapSnapshotFileUtils",
                         "devtools/shared/heapsnapshot/HeapSnapshotFileUtils");
loader.lazyRequireGetter(this, "ThreadSafeChromeUtils");

types.addDictType("AllocationsRecordingOptions", {
  // The probability we sample any given allocation when recording
  // allocations. Must be between 0.0 and 1.0. Defaults to 1.0, or sampling
  // every allocation.
  probability: "number",

  // The maximum number of of allocation events to keep in the allocations
  // log. If new allocations arrive, when we are already at capacity, the oldest
  // allocation event is lost. This number must fit in a 32 bit signed integer.
  maxLogLength: "number"
});

/**
 * An actor that returns memory usage data for its parent actor's window.
 * A tab-scoped instance of this actor will measure the memory footprint of its
 * parent tab. A global-scoped instance however, will measure the memory
 * footprint of the chrome window referenced by the root actor.
 *
 * This actor wraps the Memory module at devtools/server/performance/memory.js
 * and provides RDP definitions.
 *
 * @see devtools/server/performance/memory.js for documentation.
 */
var MemoryActor = exports.MemoryActor = protocol.ActorClass({
  typeName: "memory",

  /**
   * The set of unsolicited events the MemoryActor emits that will be sent over
   * the RDP (by protocol.js).
   */

  events: {
    // Same format as the data passed to the
    // `Debugger.Memory.prototype.onGarbageCollection` hook. See
    // `js/src/doc/Debugger/Debugger.Memory.md` for documentation.
    "garbage-collection": {
      type: "garbage-collection",
      data: Arg(0, "json"),
    },

    // Same data as the data from `getAllocations` -- only fired if
    // `autoDrain` set during `startRecordingAllocations`.
    "allocations": {
      type: "allocations",
      data: Arg(0, "json"),
    },
  },

  initialize: function (conn, parent, frameCache = new StackFrameCache()) {
    protocol.Actor.prototype.initialize.call(this, conn);

    this._onGarbageCollection = this._onGarbageCollection.bind(this);
    this._onAllocations = this._onAllocations.bind(this);
    this.bridge = new Memory(parent, frameCache);
    this.bridge.on("garbage-collection", this._onGarbageCollection);
    this.bridge.on("allocations", this._onAllocations);
  },

  destroy: function () {
    this.bridge.off("garbage-collection", this._onGarbageCollection);
    this.bridge.off("allocations", this._onAllocations);
    this.bridge.destroy();
    protocol.Actor.prototype.destroy.call(this);
  },

  attach: actorBridge("attach", {
    request: {},
    response: {
      type: "attached"
    }
  }),

  detach: actorBridge("detach", {
    request: {},
    response: {
      type: "detached"
    }
  }),

  getState: actorBridge("getState", {
    response: {
      state: RetVal(0, "string")
    }
  }),

  saveHeapSnapshot: method(function () {
    return this.bridge.saveHeapSnapshot();
  }, {
    response: {
      snapshotId: RetVal("string")
    }
  }),

  takeCensus: actorBridge("takeCensus", {
    request: {},
    response: RetVal("json")
  }),

  startRecordingAllocations: actorBridge("startRecordingAllocations", {
    request: {
      options: Arg(0, "nullable:AllocationsRecordingOptions")
    },
    response: {
      // Accept `nullable` in the case of server Gecko <= 37, handled on the front
      value: RetVal(0, "nullable:number")
    }
  }),

  stopRecordingAllocations: actorBridge("stopRecordingAllocations", {
    request: {},
    response: {
      // Accept `nullable` in the case of server Gecko <= 37, handled on the front
      value: RetVal(0, "nullable:number")
    }
  }),

  getAllocationsSettings: actorBridge("getAllocationsSettings", {
    request: {},
    response: {
      options: RetVal(0, "json")
    }
  }),

  getAllocations: actorBridge("getAllocations", {
    request: {},
    response: RetVal("json")
  }),

  forceGarbageCollection: actorBridge("forceGarbageCollection", {
    request: {},
    response: {}
  }),

  forceCycleCollection: actorBridge("forceCycleCollection", {
    request: {},
    response: {}
  }),

  measure: actorBridge("measure", {
    request: {},
    response: RetVal("json"),
  }),

  residentUnique: actorBridge("residentUnique", {
    request: {},
    response: { value: RetVal("number") }
  }),

  _onGarbageCollection: function (data) {
    if (this.conn.transport) {
      events.emit(this, "garbage-collection", data);
    }
  },

  _onAllocations: function (data) {
    if (this.conn.transport) {
      events.emit(this, "allocations", data);
    }
  },
});

exports.MemoryFront = protocol.FrontClass(MemoryActor, {
  initialize: function (client, form, rootForm = null) {
    protocol.Front.prototype.initialize.call(this, client, form);
    this._client = client;
    this.actorID = form.memoryActor;
    this.heapSnapshotFileActorID = rootForm
      ? rootForm.heapSnapshotFileActor
      : null;
    this.manage(this);
  },

  /**
   * Save a heap snapshot, transfer it from the server to the client if the
   * server and client do not share a file system, and return the local file
   * path to the heap snapshot.
   *
   * Note that this is safe to call for actors inside sandoxed child processes,
   * as we jump through the correct IPDL hoops.
   *
   * @params Boolean options.forceCopy
   *         Always force a bulk data copy of the saved heap snapshot, even when
   *         the server and client share a file system.
   *
   * @returns Promise<String>
   */
  saveHeapSnapshot: protocol.custom(Task.async(function* (options = {}) {
    const snapshotId = yield this._saveHeapSnapshotImpl();

    if (!options.forceCopy &&
        (yield HeapSnapshotFileUtils.haveHeapSnapshotTempFile(snapshotId))) {
      return HeapSnapshotFileUtils.getHeapSnapshotTempFilePath(snapshotId);
    }

    return yield this.transferHeapSnapshot(snapshotId);
  }), {
    impl: "_saveHeapSnapshotImpl"
  }),

  /**
   * Given that we have taken a heap snapshot with the given id, transfer the
   * heap snapshot file to the client. The path to the client's local file is
   * returned.
   *
   * @param {String} snapshotId
   *
   * @returns Promise<String>
   */
  transferHeapSnapshot: protocol.custom(function (snapshotId) {
    if (!this.heapSnapshotFileActorID) {
      throw new Error("MemoryFront initialized without a rootForm");
    }

    const request = this._client.request({
      to: this.heapSnapshotFileActorID,
      type: "transferHeapSnapshot",
      snapshotId
    });

    return new Promise((resolve, reject) => {
      const outFilePath =
        HeapSnapshotFileUtils.getNewUniqueHeapSnapshotTempFilePath();
      const outFile = new FileUtils.File(outFilePath);

      const outFileStream = FileUtils.openSafeFileOutputStream(outFile);
      request.on("bulk-reply", Task.async(function* ({ copyTo }) {
        yield copyTo(outFileStream);
        FileUtils.closeSafeFileOutputStream(outFileStream);
        resolve(outFilePath);
      }));
    });
  })
});
