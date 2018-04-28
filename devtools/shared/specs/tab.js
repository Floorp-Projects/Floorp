/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {types, generateActorSpec, RetVal, Option} = require("devtools/shared/protocol");

types.addDictType("tab.attach", {
  type: "string",
  threadActor: "number",
  cacheDisabled: "boolean",
  javascriptEnabled: "boolean",
  traits: "json"
});

types.addDictType("tab.detach", {
  error: "nullable:string",
  type: "nullable:string"
});

types.addDictType("tab.switchtoframe", {
  error: "nullable:string",
  message: "nullable:string"
});

types.addDictType("tab.listframes", {
  frames: "array:tab.window"
});

types.addDictType("tab.window", {
  id: "string",
  parentID: "nullable:string",
  url: "string",
  title: "string"
});

types.addDictType("tab.workers", {
  error: "nullable:string"
});

types.addDictType("tab.reload", {
  force: "boolean"
});

types.addDictType("tab.reconfigure", {
  javascriptEnabled: "nullable:boolean",
  cacheDisabled: "nullable:boolean",
  serviceWorkersTestingEnabled: "nullable:boolean",
  performReload: "nullable:boolean"
});

const tabSpec = generateActorSpec({
  typeName: "tab",

  methods: {
    attach: {
      request: {},
      response: RetVal("tab.attach")
    },
    detach: {
      request: {},
      response: RetVal("tab.detach")
    },
    focus: {
      request: {},
      response: {}
    },
    reload: {
      request: {
        options: Option(0, "tab.reload"),
      },
      response: {}
    },
    navigateTo: {
      request: {
        url: Option(0, "string"),
      },
      response: {}
    },
    reconfigure: {
      request: {
        options: Option(0, "tab.reconfigure")
      },
      response: {}
    },
    switchToFrame: {
      request: {
        windowId: Option(0, "string")
      },
      response: RetVal("tab.switchtoframe")
    },
    listFrames: {
      request: {},
      response: RetVal("tab.listframes")
    },
    listWorkers: {
      request: {},
      response: RetVal("tab.workers")
    },
    logInPage: {
      request: {
        text: Option(0, "string"),
        category: Option(0, "string"),
        flags: Option(0, "string")
      },
      response: {}
    }
  },
});

exports.tabSpec = tabSpec;
