/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Arg, RetVal, generateActorSpec } = require("devtools/shared/protocol");

const oldStyleSheetSpec = generateActorSpec({
  typeName: "old-stylesheet",

  events: {
    "property-change": {
      type: "propertyChange",
      property: Arg(0, "string"),
      value: Arg(1, "json")
    },
    "source-load": {
      type: "sourceLoad",
      source: Arg(0, "string")
    },
    "style-applied": {
      type: "styleApplied"
    }
  },

  methods: {
    toggleDisabled: {
      response: { disabled: RetVal("boolean")}
    },
    fetchSource: {},
    update: {
      request: {
        text: Arg(0, "string"),
        transition: Arg(1, "boolean")
      }
    }
  }
});

exports.oldStyleSheetSpec = oldStyleSheetSpec;

const styleEditorSpec = generateActorSpec({
  typeName: "styleeditor",

  events: {
    "document-load": {
      type: "documentLoad",
      styleSheets: Arg(0, "array:old-stylesheet")
    }
  },

  method: {
    newDocument: {},
    newStyleSheet: {
      request: { text: Arg(0, "string") },
      response: { styleSheet: RetVal("old-stylesheet") }
    }
  }
});

exports.styleEditorSpec = styleEditorSpec;
