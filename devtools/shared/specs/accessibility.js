/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/shared/protocol");
const { Arg, generateActorSpec, RetVal, types } = protocol;
// eslint-disable-next-line no-unused-vars
const { nodeSpec } = require("devtools/shared/specs/inspector");

types.addActorType("accessible");

const accessibleSpec = generateActorSpec({
  typeName: "accessible",

  events: {
    "actions-change": {
      type: "actionsChange",
      actions: Arg(0, "array:string")
    },
    "name-change": {
      type: "nameChange",
      name: Arg(0, "string"),
      parent: Arg(1, "nullable:accessible")
    },
    "value-change": {
      type: "valueChange",
      value: Arg(0, "string")
    },
    "description-change": {
      type: "descriptionChange",
      description: Arg(0, "string")
    },
    "state-change": {
      type: "stateChange",
      states: Arg(0, "array:string")
    },
    "attributes-change": {
      type: "attributesChange",
      states: Arg(0, "json")
    },
    "help-change": {
      type: "helpChange",
      help: Arg(0, "string")
    },
    "shortcut-change": {
      type: "shortcutChange",
      shortcut: Arg(0, "string")
    },
    "reorder": {
      type: "reorder",
      childCount: Arg(0, "number")
    },
    "text-change": {
      type: "textChange"
    }
  },

  methods: {
    getActions: {
      request: {},
      response: {
        actions: RetVal("array:string")
      }
    },
    getIndexInParent: {
      request: {},
      response: {
        indexInParent: RetVal("number")
      }
    },
    getState: {
      request: {},
      response: {
        states: RetVal("array:string")
      }
    },
    getAttributes: {
      request: {},
      response: {
        attributes: RetVal("json")
      }
    },
    children: {
      request: {},
      response: {
        children: RetVal("array:accessible")
      }
    }
  }
});

const accessibleWalkerSpec = generateActorSpec({
  typeName: "accessiblewalker",

  events: {
    "accessible-destroy": {
      type: "accessibleDestroy",
      accessible: Arg(0, "accessible")
    }
  },

  methods: {
    children: {
      request: {},
      response: {
        children: RetVal("array:accessible")
      }
    },
    getDocument: {
      request: {},
      response: {
        document: RetVal("accessible")
      }
    },
    getAccessibleFor: {
      request: { node: Arg(0, "domnode") },
      response: {
        accessible: RetVal("accessible")
      }
    }
  }
});

const accessibilitySpec = generateActorSpec({
  typeName: "accessibility",

  methods: {
    getWalker: {
      request: {},
      response: {
        walker: RetVal("accessiblewalker")
      }
    }
  }
});

exports.accessibleSpec = accessibleSpec;
exports.accessibleWalkerSpec = accessibleWalkerSpec;
exports.accessibilitySpec = accessibilitySpec;
