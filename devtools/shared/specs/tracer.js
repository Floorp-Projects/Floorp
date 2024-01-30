/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  generateActorSpec,
  Arg,
  types,
} = require("resource://devtools/shared/protocol.js");

types.addDictType("tracer.start.options", {
  logMethod: "string",
  traceValues: "boolean",
  traceOnNextInteraction: "boolean",
  traceOnNextLoad: "boolean",
});

const tracerSpec = generateActorSpec({
  typeName: "tracer",

  methods: {
    startTracing: {
      request: {
        options: Arg(0, "tracer.start.options"),
      },
    },
    stopTracing: {
      request: {},
    },
  },
});

exports.tracerSpec = tracerSpec;
