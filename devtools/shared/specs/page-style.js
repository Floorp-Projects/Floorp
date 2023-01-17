/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Arg,
  Option,
  RetVal,
  generateActorSpec,
  types,
} = require("resource://devtools/shared/protocol.js");

// Load the shared types for style actors
require("resource://devtools/shared/specs/style/style-types.js");

const pageStyleSpec = generateActorSpec({
  typeName: "pagestyle",

  events: {
    "stylesheet-updated": {
      type: "styleSheetUpdated",
    },
  },

  methods: {
    getComputed: {
      request: {
        node: Arg(0, "domnode"),
        markMatched: Option(1, "boolean"),
        onlyMatched: Option(1, "boolean"),
        filter: Option(1, "string"),
        filterProperties: Option(1, "nullable:array:string"),
      },
      response: {
        computed: RetVal("json"),
      },
    },
    getAllUsedFontFaces: {
      request: {
        includePreviews: Option(0, "boolean"),
        includeVariations: Option(1, "boolean"),
        previewText: Option(0, "string"),
        previewFontSize: Option(0, "string"),
        previewFillStyle: Option(0, "string"),
      },
      response: {
        fontFaces: RetVal("array:fontface"),
      },
    },
    getUsedFontFaces: {
      request: {
        node: Arg(0, "domnode"),
        includePreviews: Option(1, "boolean"),
        includeVariations: Option(1, "boolean"),
        previewText: Option(1, "string"),
        previewFontSize: Option(1, "string"),
        previewFillStyle: Option(1, "string"),
      },
      response: {
        fontFaces: RetVal("array:fontface"),
      },
    },
    getMatchedSelectors: {
      request: {
        node: Arg(0, "domnode"),
        property: Arg(1, "string"),
        filter: Option(2, "string"),
      },
      response: RetVal(
        types.addDictType("matchedselectorresponse", {
          rules: "array:domstylerule",
          matched: "array:matchedselector",
        })
      ),
    },
    getRule: {
      request: {
        ruleId: Arg(0, "string"),
      },
      response: {
        rule: RetVal("nullable:domstylerule"),
      },
    },
    getApplied: {
      request: {
        node: Arg(0, "domnode"),
        inherited: Option(1, "boolean"),
        matchedSelectors: Option(1, "boolean"),
        skipPseudo: Option(1, "boolean"),
        filter: Option(1, "string"),
      },
      response: RetVal("appliedStylesReturn"),
    },
    isPositionEditable: {
      request: { node: Arg(0, "domnode") },
      response: { value: RetVal("boolean") },
    },
    getLayout: {
      request: {
        node: Arg(0, "domnode"),
        autoMargins: Option(1, "boolean"),
      },
      response: RetVal("json"),
    },
    addNewRule: {
      request: {
        node: Arg(0, "domnode"),
        pseudoClasses: Arg(1, "nullable:array:string"),
      },
      response: RetVal("appliedStylesReturn"),
    },
    getAttributesInOwnerDocument: {
      request: {
        search: Arg(0, "string"),
        attributeType: Arg(1, "string"),
        node: Arg(2, "nullable:domnode"),
      },
      response: {
        attributes: RetVal("array:string"),
      },
    },
  },
});

exports.pageStyleSpec = pageStyleSpec;
