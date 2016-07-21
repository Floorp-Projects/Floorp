/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Arg, RetVal, generateActorSpec } = require("devtools/shared/protocol");

const eventLoopLagSpec = generateActorSpec({
  typeName: "eventLoopLag",

  events: {
    "event-loop-lag": {
      type: "event-loop-lag",
      // duration of the lag in milliseconds.
      time: Arg(0, "number")
    }
  },

  methods: {
    start: {
      request: {},
      response: {success: RetVal("number")}
    },
    stop: {
      request: {},
      response: {}
    }
  }
});

exports.eventLoopLagSpec = eventLoopLagSpec;
