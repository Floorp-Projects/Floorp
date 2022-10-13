/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Arg,
  generateActorSpec,
} = require("resource://devtools/shared/protocol.js");

const mediaRuleSpec = generateActorSpec({
  typeName: "mediarule",

  events: {
    "matches-change": {
      type: "matchesChange",
      matches: Arg(0, "boolean"),
    },
  },
});

exports.mediaRuleSpec = mediaRuleSpec;
