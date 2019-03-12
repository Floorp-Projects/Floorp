/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const protocol = require("devtools/shared/protocol");
const { RetVal, generateActorSpec } = protocol;

/**
 * Type describing a single function call in a stack trace.
 */
protocol.types.addDictType("call-stack-item", {
  name: "string",
  file: "string",
  line: "number",
});

/**
 * Type describing an overview of a function call.
 */
protocol.types.addDictType("call-details", {
  type: "number",
  name: "string",
  stack: "array:call-stack-item",
});

const functionCallSpec = generateActorSpec({
  typeName: "function-call",

  methods: {
    getDetails: {
      response: { info: RetVal("call-details") },
    },
  },
});

exports.functionCallSpec = functionCallSpec;
