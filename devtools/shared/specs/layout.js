/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Arg,
  generateActorSpec,
  RetVal,
} = require("resource://devtools/shared/protocol.js");

const flexboxSpec = generateActorSpec({
  typeName: "flexbox",

  methods: {
    getFlexItems: {
      request: {},
      response: {
        flexitems: RetVal("array:flexitem"),
      },
    },
  },
});

const flexItemSpec = generateActorSpec({
  typeName: "flexitem",

  methods: {},
});

const gridSpec = generateActorSpec({
  typeName: "grid",

  methods: {},
});

const layoutSpec = generateActorSpec({
  typeName: "layout",

  methods: {
    getCurrentFlexbox: {
      request: {
        node: Arg(0, "domnode"),
        onlyLookAtParents: Arg(1, "nullable:boolean"),
      },
      response: {
        flexbox: RetVal("nullable:flexbox"),
      },
    },

    getCurrentGrid: {
      request: {
        node: Arg(0, "domnode"),
      },
      response: {
        grid: RetVal("nullable:grid"),
      },
    },

    getGrids: {
      request: {
        rootNode: Arg(0, "domnode"),
      },
      response: {
        grids: RetVal("array:grid"),
      },
    },
  },
});

exports.flexboxSpec = flexboxSpec;
exports.flexItemSpec = flexItemSpec;
exports.gridSpec = gridSpec;
exports.layoutSpec = layoutSpec;
