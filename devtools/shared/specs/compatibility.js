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

types.addDictType("browsertype", {
  id: "string",
  name: "string",
  version: "string",
  status: "string",
});

types.addDictType("compatibilityissues", {
  type: "string",
  property: "string",
  aliases: "nullable:array:string",
  url: "nullable:string",
  specUrl: "nullable:string",
  deprecated: "boolean",
  experimental: "boolean",
  unsupportedBrowsers: "array:browsertype",
});

types.addDictType("declaration", {
  name: "string",
  value: "string",
});

const compatibilitySpec = generateActorSpec({
  typeName: "compatibility",

  methods: {
    // While not being used on the client at the moment, keep this method in case
    // we need traits again to support backwards compatibility for the Compatibility
    // actor.
    getTraits: {
      request: {},
      response: { traits: RetVal("json") },
    },
    getCSSDeclarationBlockIssues: {
      request: {
        domRulesDeclarations: Arg(0, "array:array:declaration"),
        targetBrowsers: Arg(1, "array:browsertype"),
      },
      response: {
        compatibilityIssues: RetVal("array:array:compatibilityissues"),
      },
    },
    getNodeCssIssues: {
      request: {
        node: Arg(0, "domnode"),
        targetBrowsers: Arg(1, "array:browsertype"),
      },
      response: {
        compatibilityIssues: RetVal("array:compatibilityissues"),
      },
    },
  },
});

exports.compatibilitySpec = compatibilitySpec;
