/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Arg,
  Option,
  generateActorSpec
} = require("devtools/shared/protocol");

const highlighterSpec = generateActorSpec({
  typeName: "highlighter",

  methods: {
    showBoxModel: {
      request: {
        node: Arg(0, "domnode"),
        region: Option(1),
        hideInfoBar: Option(1),
        hideGuides: Option(1),
        showOnly: Option(1),
        onlyRegionArea: Option(1)
      }
    },
    hideBoxModel: {
      request: {}
    },
    pick: {},
    cancelPick: {}
  }
});

exports.highlighterSpec = highlighterSpec;
