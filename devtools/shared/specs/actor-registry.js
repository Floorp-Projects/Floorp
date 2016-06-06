/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Arg,
  RetVal,
  generateActorSpec,
} = require("devtools/shared/protocol");

const actorActorSpec = generateActorSpec({
  typeName: "actorActor",

  methods: {
    unregister: {
      request: {},
      response: {}
    }
  },
});

exports.actorActorSpec = actorActorSpec;

const actorRegistrySpec = generateActorSpec({
  typeName: "actorRegistry",

  methods: {
    registerActor: {
      request: {
        sourceText: Arg(0, "string"),
        filename: Arg(1, "string"),
        options: Arg(2, "json")
      },

      response: {
        actorActor: RetVal("actorActor")
      }
    }
  }
});

exports.actorRegistrySpec = actorRegistrySpec;
