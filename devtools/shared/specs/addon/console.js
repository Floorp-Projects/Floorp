/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { generateActorSpec } = require("devtools/shared/protocol");
const { extend } = require("devtools/shared/extend");
const { webconsoleSpecPrototype } = require("devtools/shared/specs/webconsole");

const addonConsoleSpec = generateActorSpec(extend(webconsoleSpecPrototype, {
  typeName: "addonConsole",
}));

exports.addonConsoleSpec = addonConsoleSpec;
