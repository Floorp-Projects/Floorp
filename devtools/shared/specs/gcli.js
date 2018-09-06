/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Arg, RetVal, generateActorSpec } = require("devtools/shared/protocol");

const gcliSpec = generateActorSpec({
  typeName: "gcli",

  events: {
    "commands-changed": {
      type: "commandsChanged"
    }
  },

  methods: {
    _testOnlyAddItemsByModule: {
      request: {
        customProps: Arg(0, "array:string")
      }
    },
    _testOnlyRemoveItemsByModule: {
      request: {
        customProps: Arg(0, "array:string")
      }
    },
    specs: {
      request: {
        customProps: Arg(0, "nullable:array:string")
      },
      response: {
        value: RetVal("array:json")
      }
    },
    execute: {
      request: {
        // The command string
        typed: Arg(0, "string")
      },
      response: RetVal("json")
    },
    state: {
      request: {
        // The command string
        typed: Arg(0, "string"),
        // Cursor start position
        start: Arg(1, "number"),
        // The prediction offset (# times UP/DOWN pressed)
        rank: Arg(2, "number")
      },
      response: RetVal("json")
    },
    parseType: {
      request: {
        // The command string
        typed: Arg(0, "string"),
        // The name of the parameter to parse
        paramName: Arg(1, "string")
      },
      response: RetVal("json")
    },
    nudgeType: {
      request: {
        // The command string
        typed: Arg(0, "string"),
        // +1/-1 for increment / decrement
        by: Arg(1, "number"),
        // The name of the parameter to parse
        paramName: Arg(2, "string")
      },
      response: RetVal("string")
    },
    getSelectionLookup: {
      request: {
        // The command containing the parameter in question
        commandName: Arg(0, "string"),
        // The name of the parameter
        paramName: Arg(1, "string"),
      },
      response: RetVal("json")
    }
  }
});

exports.gcliSpec = gcliSpec;
