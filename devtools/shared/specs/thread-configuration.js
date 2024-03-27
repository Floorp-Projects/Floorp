/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  generateActorSpec,
  Arg,
  types,
} = require("resource://devtools/shared/protocol.js");

types.addDictType("thread-configuration.configuration", {
  pauseOnExceptions: "nullable:boolean",
  ignoreCaughtExceptions: "nullable:boolean",
});

const threadConfigurationSpec = generateActorSpec({
  typeName: "thread-configuration",

  methods: {
    updateConfiguration: {
      request: {
        configuration: Arg(0, "thread-configuration.configuration"),
      },
      response: {},
    },
  },
});

exports.threadConfigurationSpec = threadConfigurationSpec;
