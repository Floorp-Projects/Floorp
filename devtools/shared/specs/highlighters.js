/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Arg,
  RetVal,
  generateActorSpec,
} = require("resource://devtools/shared/protocol.js");

const customHighlighterSpec = generateActorSpec({
  typeName: "customhighlighter",

  events: {
    "highlighter-event": {
      type: "highlighter-event",
      data: Arg(0, "json"),
    },
  },

  methods: {
    release: {
      release: true,
    },
    show: {
      request: {
        node: Arg(0, "nullable:domnode"),
        options: Arg(1, "nullable:json"),
      },
      response: {
        value: RetVal("nullable:boolean"),
      },
    },
    hide: {
      request: {},
    },
    finalize: {
      oneway: true,
    },
  },
});

exports.customHighlighterSpec = customHighlighterSpec;
