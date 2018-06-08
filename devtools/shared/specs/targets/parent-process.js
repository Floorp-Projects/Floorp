/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { generateActorSpec } = require("devtools/shared/protocol");
const { extend } = require("devtools/shared/extend");
const { browsingContextTargetSpecPrototype } = require("devtools/shared/specs/targets/browsing-context");

const parentProcessTargetSpecPrototype = extend(
  browsingContextTargetSpecPrototype,
  {
    typeName: "parentProcessTarget",
  }
);

const parentProcessTargetSpec = generateActorSpec(parentProcessTargetSpecPrototype);

exports.parentProcessTargetSpecPrototype = parentProcessTargetSpecPrototype;
exports.parentProcessTargetSpec = parentProcessTargetSpec;
