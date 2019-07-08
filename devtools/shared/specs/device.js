/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Arg, RetVal, generateActorSpec } = require("devtools/shared/protocol");
const deviceSpec = generateActorSpec({
  typeName: "device",

  events: {
    "multi-e10s-updated": {
      type: "multi-e10s-updated",
      isMultiE10s: Arg(0, "boolean"),
    },
  },

  methods: {
    getDescription: { request: {}, response: { value: RetVal("json") } },
    screenshotToDataURL: {
      request: {},
      response: { value: RetVal("longstring") },
    },
  },
});

exports.deviceSpec = deviceSpec;
