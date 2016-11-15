/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Arg, generateActorSpec, RetVal } = require("devtools/shared/protocol");
require("devtools/shared/specs/node");

const gridSpec = generateActorSpec({
  typeName: "grid",

  methods: {},
});

const layoutSpec = generateActorSpec({
  typeName: "layout",

  events: {
    "grid-layout-changed": {
      type: "grid-layout-changed",
      grids: Arg(0, "array:grid")
    }
  },

  methods: {
    getAllGrids: {
      request: {
        rootNode: Arg(0, "domnode"),
        traverseFrames: Arg(1, "nullable:boolean")
      },
      response: {
        grids: RetVal("array:grid")
      }
    }
  },
});

exports.gridSpec = gridSpec;
exports.layoutSpec = layoutSpec;
