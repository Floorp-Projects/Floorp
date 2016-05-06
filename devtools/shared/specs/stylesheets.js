/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Arg,
  RetVal,
  generateActorSpec,
  types
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

types.addActorType("stylesheet");

const styleSheetSpec = generateActorSpec({
  typeName: "stylesheet",

  events: {
    "property-change": {
      type: "propertyChange",
      property: Arg(0, "string"),
      value: Arg(1, "json")
    },
    "style-applied": {
      type: "styleApplied",
      kind: Arg(0, "number"),
      styleSheet: Arg(1, "stylesheet")
    },
    "media-rules-changed": {
      type: "mediaRulesChanged",
      rules: Arg(0, "array:mediarule")
    }
  },

  methods: {
    toggleDisabled: {
      response: { disabled: RetVal("boolean")}
    },
    getText: {
      response: {
        text: RetVal("longstring")
      }
    },
    getOriginalSources: {
      request: {},
      response: {
        originalSources: RetVal("nullable:array:originalsource")
      }
    },
    getOriginalLocation: {
      request: {
        line: Arg(0, "number"),
        column: Arg(1, "number")
      },
      response: RetVal(types.addDictType("originallocationresponse", {
        source: "string",
        line: "number",
        column: "number"
      }))
    },
    getMediaRules: {
      request: {},
      response: {
        mediaRules: RetVal("nullable:array:mediarule")
      }
    },
    update: {
      request: {
        text: Arg(0, "string"),
        transition: Arg(1, "boolean")
      }
    }
  }
});

exports.styleSheetSpec = styleSheetSpec;

const styleSheetsSpec = generateActorSpec({
  typeName: "stylesheets",

  methods: {
    getStyleSheets: {
      request: {},
      response: { styleSheets: RetVal("array:stylesheet") }
    },
    addStyleSheet: {
      request: { text: Arg(0, "string") },
      response: { styleSheet: RetVal("stylesheet") }
    }
  }
});

exports.styleSheetsSpec = styleSheetsSpec;
