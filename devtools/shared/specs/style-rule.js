/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Arg, RetVal, generateActorSpec } = require("devtools/shared/protocol");

// Load the shared types for style actors
require("devtools/shared/specs/style/style-types");

const styleRuleSpec = generateActorSpec({
  typeName: "domstylerule",

  events: {
    "location-changed": {
      type: "locationChanged",
      line: Arg(0, "number"),
      column: Arg(1, "number"),
    },
    "rule-updated": {
      type: "ruleUpdated",
      rule: Arg(0, "domstylerule"),
    },
  },

  methods: {
    getRuleText: {
      response: {
        text: RetVal("string"),
      },
    },
    setRuleText: {
      request: {
        newText: Arg(0, "string"),
        modifications: Arg(1, "array:json"),
      },
      response: { rule: RetVal("domstylerule") },
    },
    modifyProperties: {
      request: { modifications: Arg(0, "array:json") },
      response: { rule: RetVal("domstylerule") },
    },
    modifySelector: {
      request: {
        node: Arg(0, "domnode"),
        value: Arg(1, "string"),
        editAuthored: Arg(2, "boolean"),
      },
      response: RetVal("modifiedStylesReturn"),
    },
  },
});

exports.styleRuleSpec = styleRuleSpec;

// The PageStyle actor flattens the DOM CSS objects a little bit, merging
// Rules and their Styles into one actor.  For elements (which have a style
// but no associated rule) we fake a rule with the following style id.
const ELEMENT_STYLE = 100;
exports.ELEMENT_STYLE = ELEMENT_STYLE;
