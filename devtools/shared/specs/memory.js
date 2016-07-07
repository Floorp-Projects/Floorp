/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Arg,
  RetVal,
  types,
  generateActorSpec,
} = require("devtools/shared/protocol");

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

const memorySpec = generateActorSpec({
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

  methods: {
    attach: {
      request: {},
      response: {
        type: "attached"
      }
    },
    detach: {
      request: {},
      response: {
        type: "detached"
      }
    },
    getState: {
      response: {
        state: RetVal(0, "string")
      }
    },
    takeCensus: {
      request: {},
      response: RetVal("json")
    },
    startRecordingAllocations: {
      request: {
        options: Arg(0, "nullable:AllocationsRecordingOptions")
      },
      response: {
        // Accept `nullable` in the case of server Gecko <= 37, handled on the front
        value: RetVal(0, "nullable:number")
      }
    },
    stopRecordingAllocations: {
      request: {},
      response: {
        // Accept `nullable` in the case of server Gecko <= 37, handled on the front
        value: RetVal(0, "nullable:number")
      }
    },
    getAllocationsSettings: {
      request: {},
      response: {
        options: RetVal(0, "json")
      }
    },
    getAllocations: {
      request: {},
      response: RetVal("json")
    },
    forceGarbageCollection: {
      request: {},
      response: {}
    },
    forceCycleCollection: {
      request: {},
      response: {}
    },
    measure: {
      request: {},
      response: RetVal("json"),
    },
    residentUnique: {
      request: {},
      response: { value: RetVal("number") }
    },
    saveHeapSnapshot: {
      request: {
        boundaries: Arg(0, "nullable:json")
      },
      response: {
        snapshotId: RetVal("string")
      }
    },
  },
});

exports.memorySpec = memorySpec;
