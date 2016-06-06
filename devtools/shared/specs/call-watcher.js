/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const protocol = require("devtools/shared/protocol");
const { Arg, RetVal, Option, generateActorSpec } = protocol;

/**
 * Type describing a single function call in a stack trace.
 */
protocol.types.addDictType("call-stack-item", {
  name: "string",
  file: "string",
  line: "number"
});

/**
 * Type describing an overview of a function call.
 */
protocol.types.addDictType("call-details", {
  type: "number",
  name: "string",
  stack: "array:call-stack-item"
});

const functionCallSpec = generateActorSpec({
  typeName: "function-call",

  methods: {
    getDetails: {
      response: { info: RetVal("call-details") }
    },
  },
});

exports.functionCallSpec = functionCallSpec;

const callWatcherSpec = generateActorSpec({
  typeName: "call-watcher",

  events: {
    /**
     * Events emitted when the `onCall` function isn't provided.
     */
    "call": {
      type: "call",
      function: Arg(0, "function-call")
    }
  },

  methods: {
    setup: {
      request: {
        tracedGlobals: Option(0, "nullable:array:string"),
        tracedFunctions: Option(0, "nullable:array:string"),
        startRecording: Option(0, "boolean"),
        performReload: Option(0, "boolean"),
        holdWeak: Option(0, "boolean"),
        storeCalls: Option(0, "boolean")
      },
      oneway: true
    },
    finalize: {
      oneway: true
    },
    isRecording: {
      response: RetVal("boolean")
    },
    initTimestampEpoch: {},
    resumeRecording: {},
    pauseRecording: {
      response: { calls: RetVal("array:function-call") }
    },
    eraseRecording: {},
  }
});

exports.callWatcherSpec = callWatcherSpec;
