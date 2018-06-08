/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { generateActorSpec } = require("devtools/shared/protocol");
const { extend } = require("devtools/shared/extend");
const { browsingContextTargetSpecPrototype } = require("devtools/shared/specs/targets/browsing-context");

// Bug 1467560: Update `generateActorSpec` support extension more naturally
const frameTargetSpec = generateActorSpec(extend(browsingContextTargetSpecPrototype, {
  typeName: "frameTarget",
}));

exports.frameTargetSpec = frameTargetSpec;
