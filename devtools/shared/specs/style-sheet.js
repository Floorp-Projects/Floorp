/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Arg,
  RetVal,
  generateActorSpec,
  types,
} = require("resource://devtools/shared/protocol.js");

types.addActorType("stylesheet");

const styleSheetSpec = generateActorSpec({
  typeName: "stylesheet",

  events: {
    "property-change": {
      type: "propertyChange",
      property: Arg(0, "string"),
      value: Arg(1, "json"),
    },
    "style-applied": {
      type: "styleApplied",
      kind: Arg(0, "number"),
      styleSheet: Arg(1, "stylesheet"),
      cause: Arg(2, "nullable:string"),
    },
  },

  methods: {
    // This is only called from StyleSheetFront#guessIndentation, which is only called
    // from RuleRewriter#getDefaultIndentation when the rule's parent stylesheet isn't
    // a resource. Once we support StyleSheet resource everywhere, this method can be
    // removed (See Bug 1672090 for more information).
    getText: {
      response: {
        text: RetVal("longstring"),
      },
    },
  },
});

exports.styleSheetSpec = styleSheetSpec;
