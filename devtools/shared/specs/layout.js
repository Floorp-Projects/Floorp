/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Arg, generateActorSpec, RetVal } = require("devtools/shared/protocol");

const flexboxSpec = generateActorSpec({
  typeName: "flexbox",

  methods: {},
});

const gridSpec = generateActorSpec({
  typeName: "grid",

  methods: {},
});

const layoutSpec = generateActorSpec({
  typeName: "layout",

  methods: {
    getAllFlexbox: {
      request: {
        rootNode: Arg(0, "domnode"),
        traverseFrames: Arg(1, "nullable:boolean")
      },
      response: {
        flexboxes: RetVal("array:flexbox")
      }
    },

    getAllGrids: {
      request: {
        rootNode: Arg(0, "domnode"),
        traverseFrames: Arg(1, "nullable:boolean")
      },
      response: {
        grids: RetVal("array:grid")
      }
    },
  },
});

exports.flexboxSpec = flexboxSpec;
exports.gridSpec = gridSpec;
exports.layoutSpec = layoutSpec;
