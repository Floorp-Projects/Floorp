/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Arg,
  RetVal,
  generateActorSpec,
  types,
} = require("devtools/shared/protocol");

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
  url: "string",
  deprecated: "boolean",
  experimental: "boolean",
  unsupportedBrowsers: "array:browsertype",
});

const compatibilitySpec = generateActorSpec({
  typeName: "compatibility",

  methods: {
    getNodeCssIssues: {
      request: {
        node: Arg(0, "domnode"),
        targetBrowsers: Arg(1, "array:browsertype"),
      },
      response: RetVal("array:compatibilityissues"),
    },
  },
});

exports.compatibilitySpec = compatibilitySpec;
