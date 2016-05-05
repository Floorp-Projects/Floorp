/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Arg,
  RetVal,
  generateActorSpec
} = require("devtools/server/protocol.js");

const originalSourceSpec = generateActorSpec({
  typeName: "originalsource",

  methods: {
    getText: {
      response: {
        text: RetVal("longstring")
      }
    }
  }
});

exports.originalSourceSpec = originalSourceSpec;

const mediaRuleSpec = generateActorSpec({
  typeName: "mediarule",

  events: {
    "matches-change": {
      type: "matchesChange",
      matches: Arg(0, "boolean"),
    }
  }
});

exports.mediaRuleSpec = mediaRuleSpec;
