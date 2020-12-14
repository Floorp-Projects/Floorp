/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { generateActorSpec, Arg } = require("devtools/shared/protocol");

const breakpointListSpec = generateActorSpec({
  typeName: "breakpoint-list",

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
  },
});

exports.breakpointListSpec = breakpointListSpec;
