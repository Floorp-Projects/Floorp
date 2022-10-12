/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  generateActorSpec,
  Arg,
  types,
} = require("resource://devtools/shared/protocol.js");

types.addDictType("breakpoint-list.breakpoint-options", {
  condition: "nullable:string",
  logValue: "nullable:string",
});

const breakpointListSpec = generateActorSpec({
  typeName: "breakpoint-list",

  methods: {
    setBreakpoint: {
      request: {
        location: Arg(0, "json"),
        options: Arg(1, "breakpoint-list.breakpoint-options"),
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
    },
    removeXHRBreakpoint: {
      request: {
        path: Arg(0, "string"),
        method: Arg(1, "string"),
      },
    },
    setActiveEventBreakpoints: {
      request: {
        ids: Arg(0, "array:string"),
      },
    },
  },
});

exports.breakpointListSpec = breakpointListSpec;
