/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { generateActorSpec } = require("devtools/shared/protocol");

// XXX: This actor doesn't expose any methods and events yet, but will in the future
// (e.g. resource-available-form and resource-destroyed-form).

const workerTargetSpec = generateActorSpec({
  typeName: "workerTarget",
  methods: {},
  events: {},
});

exports.workerTargetSpec = workerTargetSpec;
