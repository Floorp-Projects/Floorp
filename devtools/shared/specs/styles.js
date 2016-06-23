/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Arg,
  Option,
  RetVal,
  generateActorSpec,
  types
} = require("devtools/shared/protocol");
require("devtools/shared/specs/node");
require("devtools/shared/specs/stylesheets");

// Predeclare the domstylerule actor type
types.addActorType("domstylerule");

/**
 * DOM Nodes returned by the style actor will be owned by the DOM walker
 * for the connection.
  */
types.addLifetime("walker", "walker");

/**
 * When asking for the styles applied to a node, we return a list of
 * appliedstyle json objects that lists the rules that apply to the node
 * and which element they were inherited from (if any).
 *
 * Note appliedstyle only sends the list of actorIDs and is not a valid return
 * value on its own. appliedstyle should be returned with the actual list of
 * StyleRuleActor and StyleSheetActor. See appliedStylesReturn.
 */
types.addDictType("appliedstyle", {
  rule: "domstylerule#actorid",
  inherited: "nullable:domnode#actorid",
  keyframes: "nullable:domstylerule#actorid"
});

types.addDictType("matchedselector", {
  rule: "domstylerule#actorid",
  selector: "string",
  value: "string",
  status: "number"
});

types.addDictType("appliedStylesReturn", {
  entries: "array:appliedstyle",
  rules: "array:domstylerule",
  sheets: "array:stylesheet"
});

types.addDictType("modifiedStylesReturn", {
  isMatching: RetVal("boolean"),
  ruleProps: RetVal("nullable:appliedStylesReturn")
});

types.addDictType("fontpreview", {
  data: "nullable:longstring",
  size: "json"
});

types.addDictType("fontface", {
  name: "string",
  CSSFamilyName: "string",
  rule: "nullable:domstylerule",
  srcIndex: "number",
  URI: "string",
  format: "string",
  preview: "nullable:fontpreview",
  localName: "string",
  metadata: "string"
});

const pageStyleSpec = generateActorSpec({
  typeName: "pagestyle",

  events: {
    "stylesheet-updated": {
      type: "styleSheetUpdated",
      styleSheet: Arg(0, "stylesheet")
    }
  },

  methods: {
    getComputed: {
      request: {
        node: Arg(0, "domnode"),
        markMatched: Option(1, "boolean"),
        onlyMatched: Option(1, "boolean"),
        filter: Option(1, "string"),
      },
      response: {
        computed: RetVal("json")
      }
    },
    getAllUsedFontFaces: {
      request: {
        includePreviews: Option(0, "boolean"),
        previewText: Option(0, "string"),
        previewFontSize: Option(0, "string"),
        previewFillStyle: Option(0, "string")
      },
      response: {
        fontFaces: RetVal("array:fontface")
      }
    },
    getUsedFontFaces: {
      request: {
        node: Arg(0, "domnode"),
        includePreviews: Option(1, "boolean"),
        previewText: Option(1, "string"),
        previewFontSize: Option(1, "string"),
        previewFillStyle: Option(1, "string")
      },
      response: {
        fontFaces: RetVal("array:fontface")
      }
    },
    getMatchedSelectors: {
      request: {
        node: Arg(0, "domnode"),
        property: Arg(1, "string"),
        filter: Option(2, "string")
      },
      response: RetVal(types.addDictType("matchedselectorresponse", {
        rules: "array:domstylerule",
        sheets: "array:stylesheet",
        matched: "array:matchedselector"
      }))
    },
    getApplied: {
      request: {
        node: Arg(0, "domnode"),
        inherited: Option(1, "boolean"),
        matchedSelectors: Option(1, "boolean"),
        filter: Option(1, "string")
      },
      response: RetVal("appliedStylesReturn")
    },
    isPositionEditable: {
      request: { node: Arg(0, "domnode")},
      response: { value: RetVal("boolean") }
    },
    getLayout: {
      request: {
        node: Arg(0, "domnode"),
        autoMargins: Option(1, "boolean")
      },
      response: RetVal("json")
    },
    addNewRule: {
      request: {
        node: Arg(0, "domnode"),
        pseudoClasses: Arg(1, "nullable:array:string"),
        editAuthored: Arg(2, "boolean")
      },
      response: RetVal("appliedStylesReturn")
    }
  }
});

exports.pageStyleSpec = pageStyleSpec;

const styleRuleSpec = generateActorSpec({
  typeName: "domstylerule",

  events: {
    "location-changed": {
      type: "locationChanged",
      line: Arg(0, "number"),
      column: Arg(1, "number")
    },
  },

  methods: {
    setRuleText: {
      request: { modification: Arg(0, "string") },
      response: { rule: RetVal("domstylerule") }
    },
    modifyProperties: {
      request: { modifications: Arg(0, "array:json") },
      response: { rule: RetVal("domstylerule") }
    },
    modifySelector: {
      request: { selector: Arg(0, "string") },
      response: { isModified: RetVal("boolean") },
    },
    modifySelector2: {
      request: {
        node: Arg(0, "domnode"),
        value: Arg(1, "string"),
        editAuthored: Arg(2, "boolean")
      },
      response: RetVal("modifiedStylesReturn")
    }
  }
});

exports.styleRuleSpec = styleRuleSpec;

// The PageStyle actor flattens the DOM CSS objects a little bit, merging
// Rules and their Styles into one actor.  For elements (which have a style
// but no associated rule) we fake a rule with the following style id.
const ELEMENT_STYLE = 100;
exports.ELEMENT_STYLE = ELEMENT_STYLE;
