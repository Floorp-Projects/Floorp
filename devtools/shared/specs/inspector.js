/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Arg, RetVal, generateActorSpec } = require("devtools/shared/protocol");

const inspectorSpec = generateActorSpec({
  typeName: "inspector",

  events: {
    "color-picked": {
      type: "colorPicked",
      color: Arg(0, "string"),
    },
    "color-pick-canceled": {
      type: "colorPickCanceled",
    },
  },

  methods: {
    getWalker: {
      request: {
        options: Arg(0, "nullable:json"),
      },
      response: {
        walker: RetVal("domwalker"),
      },
    },
    getPageStyle: {
      request: {},
      response: {
        pageStyle: RetVal("pagestyle"),
      },
    },
    getHighlighter: {
      request: {
        autohide: Arg(0, "boolean"),
      },
      response: {
        highligter: RetVal("highlighter"),
      },
    },
    getHighlighterByType: {
      request: {
        typeName: Arg(0),
      },
      response: {
        highlighter: RetVal("nullable:customhighlighter"),
      },
    },
    getImageDataFromURL: {
      request: { url: Arg(0), maxDim: Arg(1, "nullable:number") },
      response: RetVal("imageData"),
    },
    resolveRelativeURL: {
      request: { url: Arg(0, "string"), node: Arg(1, "nullable:domnode") },
      response: { value: RetVal("string") },
    },
    pickColorFromPage: {
      request: { options: Arg(0, "nullable:json") },
      response: {},
    },
    cancelPickColorFromPage: {
      request: {},
      response: {},
    },
    supportsHighlighters: {
      request: {},
      response: {
        value: RetVal("boolean"),
      },
    },
  },
});

exports.inspectorSpec = inspectorSpec;
