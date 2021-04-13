/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  types,
  generateActorSpec,
  RetVal,
  Option,
  Arg,
} = require("devtools/shared/protocol");

types.addDictType("browsingContextTarget.attach", {
  threadActor: "number",
  cacheDisabled: "boolean",
  javascriptEnabled: "boolean",
  traits: "json",
});

types.addDictType("browsingContextTarget.switchtoframe", {
  message: "string",
});

types.addDictType("browsingContextTarget.listframes", {
  frames: "array:browsingContextTarget.window",
});

types.addDictType("browsingContextTarget.window", {
  id: "string",
  parentID: "nullable:string",
  url: "nullable:string", // should be present if not destroying
  title: "nullable:string", // should be present if not destroying
  destroy: "nullable:boolean", // not present if not destroying
});

types.addDictType("browsingContextTarget.workers", {
  workers: "array:workerDescriptor",
});

types.addDictType("browsingContextTarget.reload", {
  force: "boolean",
});

// @backward-compat { version 87 } See backward-compat note for `reconfigure`.
types.addDictType("browsingContextTarget.reconfigure", {
  cacheDisabled: "nullable:boolean",
  colorSchemeSimulation: "nullable:string",
  javascriptEnabled: "nullable:boolean",
  paintFlashing: "nullable:boolean",
  printSimulationEnabled: "nullable:boolean",
  restoreFocus: "nullable:boolean",
  serviceWorkersTestingEnabled: "nullable:boolean",
});

const browsingContextTargetSpecPrototype = {
  typeName: "browsingContextTarget",

  methods: {
    attach: {
      request: {},
      response: RetVal("browsingContextTarget.attach"),
    },
    detach: {
      request: {},
      response: {},
    },
    ensureCSSErrorReportingEnabled: {
      request: {},
      response: {},
    },
    focus: {
      request: {},
      response: {},
    },
    goForward: {
      request: {},
      response: {},
    },
    goBack: {
      request: {},
      response: {},
    },
    reload: {
      request: {
        options: Option(0, "browsingContextTarget.reload"),
      },
      response: {},
    },
    navigateTo: {
      request: {
        url: Option(0, "string"),
      },
      response: {},
    },
    // @backward-compat { version 87 } Starting with version 87, targets which
    // support the watcher will rely on the configuration actor to update their
    // configuration flags. However we need to keep this request until all
    // browsing context targets support the watcher (eg webextensions).
    reconfigure: {
      request: {
        options: Option(0, "browsingContextTarget.reconfigure"),
      },
      response: {},
    },
    switchToFrame: {
      request: {
        windowId: Option(0, "string"),
      },
      response: RetVal("browsingContextTarget.switchtoframe"),
    },
    listFrames: {
      request: {},
      response: RetVal("browsingContextTarget.listframes"),
    },
    listWorkers: {
      request: {},
      response: RetVal("browsingContextTarget.workers"),
    },
    logInPage: {
      request: {
        text: Option(0, "string"),
        category: Option(0, "string"),
        flags: Option(0, "string"),
      },
      response: {},
    },
  },
  events: {
    tabNavigated: {
      type: "tabNavigated",
      url: Option(0, "string"),
      title: Option(0, "string"),
      nativeConsoleAPI: Option(0, "boolean"),
      state: Option(0, "string"),
      isFrameSwitching: Option(0, "boolean"),
    },
    frameUpdate: {
      type: "frameUpdate",
      frames: Option(0, "nullable:array:browsingContextTarget.window"),
      selected: Option(0, "nullable:number"),
      destroyAll: Option(0, "nullable:boolean"),
    },
    workerListChanged: {
      type: "workerListChanged",
    },

    "resource-available-form": {
      type: "resource-available-form",
      resources: Arg(0, "array:json"),
    },
    "resource-destroyed-form": {
      type: "resource-destroyed-form",
      resources: Arg(0, "array:json"),
    },
    "resource-updated-form": {
      type: "resource-updated-form",
      resources: Arg(0, "array:json"),
    },
  },
};

const browsingContextTargetSpec = generateActorSpec(
  browsingContextTargetSpecPrototype
);

exports.browsingContextTargetSpecPrototype = browsingContextTargetSpecPrototype;
exports.browsingContextTargetSpec = browsingContextTargetSpec;
