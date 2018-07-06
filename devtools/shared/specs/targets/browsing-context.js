/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { types, generateActorSpec, RetVal, Option } = require("devtools/shared/protocol");

types.addDictType("browsingContextTarget.attach", {
  type: "string",
  threadActor: "number",
  cacheDisabled: "boolean",
  javascriptEnabled: "boolean",
  traits: "json"
});

types.addDictType("browsingContextTarget.detach", {
  error: "nullable:string",
  type: "nullable:string"
});

types.addDictType("browsingContextTarget.switchtoframe", {
  error: "nullable:string",
  message: "nullable:string"
});

types.addDictType("browsingContextTarget.listframes", {
  frames: "array:browsingContextTarget.window"
});

types.addDictType("browsingContextTarget.window", {
  id: "string",
  parentID: "nullable:string",
  url: "nullable:string", // should be present if not destroying
  title: "nullable:string", // should be present if not destroying
  destroy: "nullable:boolean" // not present if not destroying
});

types.addDictType("browsingContextTarget.workers", {
  error: "nullable:string"
});

types.addDictType("browsingContextTarget.reload", {
  force: "boolean"
});

types.addDictType("browsingContextTarget.reconfigure", {
  javascriptEnabled: "nullable:boolean",
  cacheDisabled: "nullable:boolean",
  serviceWorkersTestingEnabled: "nullable:boolean",
  performReload: "nullable:boolean"
});

const browsingContextTargetSpecPrototype = {
  typeName: "browsingContextTarget",

  methods: {
    attach: {
      request: {},
      response: RetVal("browsingContextTarget.attach")
    },
    detach: {
      request: {},
      response: RetVal("browsingContextTarget.detach")
    },
    ensureCSSErrorReportingEnabled: {
      request: {},
      response: {}
    },
    focus: {
      request: {},
      response: {}
    },
    reload: {
      request: {
        options: Option(0, "browsingContextTarget.reload"),
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
        options: Option(0, "browsingContextTarget.reconfigure")
      },
      response: {}
    },
    switchToFrame: {
      request: {
        windowId: Option(0, "string")
      },
      response: RetVal("browsingContextTarget.switchtoframe")
    },
    listFrames: {
      request: {},
      response: RetVal("browsingContextTarget.listframes")
    },
    listWorkers: {
      request: {},
      response: RetVal("browsingContextTarget.workers")
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
  events: {
    tabNavigated: {
      type: "tabNavigated",
      url: Option(0, "string"),
      title: Option(0, "string"),
      nativeConsoleAPI: Option(0, "boolean"),
      state: Option(0, "string"),
      isFrameSwitching: Option(0, "boolean")
    },
    frameUpdate: {
      type: "frameUpdate",
      frames: Option(0, "nullable:array:browsingContextTarget.window"),
      selected: Option(0, "nullable:number"),
      destroyAll: Option(0, "nullable:boolean")
    },
    tabDetached: {
      type: "tabDetached"
    },
    workerListChanged: {
      type: "workerListChanged"
    }
  }
};

const browsingContextTargetSpec = generateActorSpec(browsingContextTargetSpecPrototype);

exports.browsingContextTargetSpecPrototype = browsingContextTargetSpecPrototype;
exports.browsingContextTargetSpec = browsingContextTargetSpec;
