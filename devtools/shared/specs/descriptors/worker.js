/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  RetVal,
  generateActorSpec,
} = require("resource://devtools/shared/protocol.js");

const workerDescriptorSpec = generateActorSpec({
  typeName: "workerDescriptor",

  methods: {
    detach: {
      request: {},
      response: {},
    },
    getTarget: {
      request: {},
      response: RetVal("json"),
    },
  },

  events: {
    "descriptor-destroyed": {
      type: "descriptor-destroyed",
    },
  },
});

exports.workerDescriptorSpec = workerDescriptorSpec;
