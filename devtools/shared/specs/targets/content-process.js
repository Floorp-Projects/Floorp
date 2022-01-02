/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  types,
  Arg,
  Option,
  RetVal,
  generateActorSpec,
} = require("devtools/shared/protocol");

types.addDictType("contentProcessTarget.workers", {
  error: "nullable:string",
  workers: "nullable:array:workerDescriptor",
});

const contentProcessTargetSpec = generateActorSpec({
  typeName: "contentProcessTarget",

  methods: {
    listWorkers: {
      request: {},
      response: RetVal("contentProcessTarget.workers"),
    },

    pauseMatchingServiceWorkers: {
      request: {
        origin: Option(0, "string"),
      },
      response: {},
    },
  },

  events: {
    workerListChanged: {
      type: "workerListChanged",
    },
    "resource-available-form": {
      type: "resource-available-form",
      resources: Arg(0, "array:json"),
    },
    "resource-destroyed-form": {
      type: "resource-destroyed-form",
      resources: Arg(0, "array:json"),
    },
    "resource-updated-form": {
      type: "resource-updated-form",
      resources: Arg(0, "array:json"),
    },
  },
});

exports.contentProcessTargetSpec = contentProcessTargetSpec;
