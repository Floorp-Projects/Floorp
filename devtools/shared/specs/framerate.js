/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Arg, RetVal, generateActorSpec } = require("devtools/shared/protocol");

const framerateSpec = generateActorSpec({
  typeName: "framerate",

  methods: {
    startRecording: {},
    stopRecording: {
      request: {
        beginAt: Arg(0, "nullable:number"),
        endAt: Arg(1, "nullable:number")
      },
      response: { ticks: RetVal("array:number") }
    },
    cancelRecording: {},
    isRecording: {
      response: { recording: RetVal("boolean") }
    },
    getPendingTicks: {
      request: {
        beginAt: Arg(0, "nullable:number"),
        endAt: Arg(1, "nullable:number")
      },
      response: { ticks: RetVal("array:number") }
    }
  }
});

exports.framerateSpec = framerateSpec;
