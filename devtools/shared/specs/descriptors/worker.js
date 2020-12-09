/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { RetVal, generateActorSpec } = require("devtools/shared/protocol");

const workerDescriptorSpec = generateActorSpec({
  typeName: "workerDescriptor",

  methods: {
    attach: {
      request: {},
      response: RetVal("json"),
    },
    detach: {
      request: {},
      response: RetVal("json"),
    },
    getTarget: {
      request: {},
      response: RetVal("json"),
    },
    push: {
      request: {},
      response: RetVal("json"),
    },
  },

  events: {
    // WorkerDescriptorActor still uses old sendActorEvent function,
    // but it should use emit instead.
    // Do not emit a `close` event as Target class emit this event on destroy
    "worker-close": {
      type: "close",
    },
  },
});

exports.workerDescriptorSpec = workerDescriptorSpec;
