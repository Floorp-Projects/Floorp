/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { RetVal, generateActorSpec } = require("devtools/shared/protocol");

const workerDescriptorSpec = generateActorSpec({
  typeName: "workerDescriptor",

  methods: {
    // @backward-compat { version 102 } WorkerDescriptor no longer implement attach method
    //                  We can remove this method once 102 is the release channel.
    attach: {
      request: {},
      response: RetVal("json"),
    },
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
