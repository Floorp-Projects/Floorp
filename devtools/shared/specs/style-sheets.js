/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Arg,
  RetVal,
  generateActorSpec,
} = require("resource://devtools/shared/protocol.js");

const styleSheetsSpec = generateActorSpec({
  typeName: "stylesheets",

  events: {
    // @backward-compat { version 111 } This event is no longer emitted by
    // recent servers, keep the definition so that protocol.js recognizes the
    // event payloads coming from older servers. Can be removed when 111 reaches
    // the release channel.
    "stylesheet-added": {
      type: "stylesheetAdded",
    },
  },

  methods: {
    getTraits: {
      request: {},
      response: { traits: RetVal("json") },
    },
    addStyleSheet: {
      request: {
        text: Arg(0, "string"),
        fileName: Arg(1, "nullable:string"),
      },
      response: {},
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
        cause: Arg(3, "nullable:string"),
      },
      response: {},
    },
  },
});

exports.styleSheetsSpec = styleSheetsSpec;
