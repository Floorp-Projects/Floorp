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
    // @backward-compat { version 88 } Fx88 now emits descriptor-destroyed event,
    // which is generic to all descriptor actors and listened from the DescriptorFrontMixin.
    // Once we support 88+, we can remove this event.
    //
    // WorkerDescriptorActor still uses old sendActorEvent function,
    // but it should use emit instead.
    // Do not emit a `close` event as Target class emit this event on destroy
    "worker-close": {
      type: "close",
    },

    "descriptor-destroyed": {
      type: "descriptor-destroyed",
    },
  },
});

exports.workerDescriptorSpec = workerDescriptorSpec;
