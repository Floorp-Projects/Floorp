/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Arg, RetVal, generateActorSpec } = require("devtools/shared/protocol");

// Load the "stylesheet" type used in this file.
require("devtools/shared/specs/style-sheet");

const styleSheetsSpec = generateActorSpec({
  typeName: "stylesheets",

  events: {
    "stylesheet-added": {
      type: "stylesheetAdded",
      sheet: Arg(0, "stylesheet"),
      isNew: Arg(1, "boolean"),
      fileName: Arg(2, "nullable:string"),
    },
  },

  methods: {
    getTraits: {
      request: {},
      response: { traits: RetVal("json") },
    },
    getStyleSheets: {
      request: {},
      response: { styleSheets: RetVal("array:stylesheet") },
    },
    addStyleSheet: {
      request: {
        text: Arg(0, "string"),
        fileName: Arg(1, "nullable:string"),
      },
      response: { styleSheet: RetVal("nullable:stylesheet") },
    },
    toggleDisabled: {
      request: { resourceId: Arg(0, "string") },
      response: { disabled: RetVal("boolean") },
    },
    getText: {
      request: { resourceId: Arg(0, "string") },
      response: { text: RetVal("longstring") },
    },
    update: {
      request: {
        resourceId: Arg(0, "string"),
        text: Arg(1, "string"),
        transition: Arg(2, "boolean"),
      },
      response: {},
    },
  },
});

exports.styleSheetsSpec = styleSheetsSpec;
