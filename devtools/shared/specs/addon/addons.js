/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Arg,
  RetVal,
  generateActorSpec,
} = require("resource://devtools/shared/protocol.js");

const addonsSpec = generateActorSpec({
  typeName: "addons",

  methods: {
    installTemporaryAddon: {
      request: {
        addonPath: Arg(0, "string"),
        openDevTools: Arg(1, "nullable:boolean"),
      },
      response: { addon: RetVal("json") },
    },

    uninstallAddon: {
      request: {
        addonId: Arg(0, "string"),
      },
      response: {},
    },
  },
});

exports.addonsSpec = addonsSpec;
