/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const protocol = require("devtools/shared/protocol");
const { Arg, RetVal, Option, generateActorSpec } = protocol;

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
