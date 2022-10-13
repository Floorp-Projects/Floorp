/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  RetVal,
  generateActorSpec,
  Option,
} = require("resource://devtools/shared/protocol.js");

const webExtensionDescriptorSpec = generateActorSpec({
  typeName: "webExtensionDescriptor",

  methods: {
    reload: {
      request: {},
      response: { addon: RetVal("json") },
    },

    terminateBackgroundScript: {
      request: {},
      response: {},
    },

    // @backward-compat { version 70 } The method is now called getTarget
    connect: {
      request: {},
      response: { form: RetVal("json") },
    },

    getTarget: {
      request: {},
      response: { form: RetVal("json") },
    },

    reloadDescriptor: {
      request: {
        bypassCache: Option(0, "boolean"),
      },
      response: {},
    },
    getWatcher: {
      request: {
        isServerTargetSwitchingEnabled: Option(0, "boolean"),
      },
      response: RetVal("watcher"),
    },
  },

  events: {
    "descriptor-destroyed": {
      type: "descriptor-destroyed",
    },
  },
});

exports.webExtensionDescriptorSpec = webExtensionDescriptorSpec;
