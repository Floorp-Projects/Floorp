/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Arg, RetVal, generateActorSpec, types} = require("devtools/shared/protocol");

types.addDictType("available-breakpoint-group", {
  name: "string",
  events: "array:available-breakpoint-event",
});
types.addDictType("available-breakpoint-event", {
  id: "string",
  name: "string",
});

const threadSpec = generateActorSpec({
  typeName: "context",

  methods: {
    setBreakpoint: {
      request: {
        location: Arg(0, "json"),
        options: Arg(1, "json"),
      },
    },
    removeBreakpoint: {
      request: {
        location: Arg(0, "json"),
      },
    },
    setXHRBreakpoint: {
      request: {
        path: Arg(0, "string"),
        method: Arg(1, "string"),
      },
      response: {
        value: RetVal("boolean"),
      },
    },
    removeXHRBreakpoint: {
      request: {
        path: Arg(0, "string"),
        method: Arg(1, "string"),
      },
      response: {
        value: RetVal("boolean"),
      },
    },
    getAvailableEventBreakpoints: {
      response: {
        value: RetVal("array:available-breakpoint-group"),
      },
    },
    getActiveEventBreakpoints: {
      response: {
        ids: RetVal("array:string"),
      },
    },
    setActiveEventBreakpoints: {
      request: {
        ids: Arg(0, "array:string"),
      },
    },
    pauseOnExceptions: {
      request: {
        pauseOnExceptions: Arg(0, "string"),
        ignoreCaughtExceptions: Arg(1, "string"),
      },
    },
  },
});

exports.threadSpec = threadSpec;
