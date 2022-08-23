/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Arg, generateActorSpec } = require("devtools/shared/protocol");

const workerTargetSpec = generateActorSpec({
  typeName: "workerTarget",
  methods: {},
  events: {
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

exports.workerTargetSpec = workerTargetSpec;
