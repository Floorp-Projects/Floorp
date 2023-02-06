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
} = require("resource://devtools/shared/protocol.js");

types.addDictType("windowGlobalTarget.switchtoframe", {
  message: "string",
});

types.addDictType("windowGlobalTarget.listframes", {
  frames: "array:windowGlobalTarget.window",
});

types.addDictType("windowGlobalTarget.window", {
  id: "string",
  parentID: "nullable:string",
  url: "nullable:string", // should be present if not destroying
  title: "nullable:string", // should be present if not destroying
  destroy: "nullable:boolean", // not present if not destroying
});

types.addDictType("windowGlobalTarget.workers", {
  workers: "array:workerDescriptor",
});

// @backward-compat { legacy }
//                  reload is preserved for third party tools. See Bug 1717837.
//                  DevTools should use Descriptor::reloadDescriptor instead.
types.addDictType("windowGlobalTarget.reload", {
  force: "boolean",
});

// @backward-compat { version 87 } See backward-compat note for `reconfigure`.
types.addDictType("windowGlobalTarget.reconfigure", {
  cacheDisabled: "nullable:boolean",
  colorSchemeSimulation: "nullable:string",
  printSimulationEnabled: "nullable:boolean",
  restoreFocus: "nullable:boolean",
  serviceWorkersTestingEnabled: "nullable:boolean",
});

const windowGlobalTargetSpecPrototype = {
  typeName: "windowGlobalTarget",

  methods: {
    detach: {
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
    // @backward-compat { legacy }
    //                  reload is preserved for third party tools. See Bug 1717837.
    //                  DevTools should use Descriptor::reloadDescriptor instead.
    reload: {
      request: {
        options: Option(0, "windowGlobalTarget.reload"),
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
        options: Option(0, "windowGlobalTarget.reconfigure"),
      },
      response: {},
    },
    switchToFrame: {
      request: {
        windowId: Option(0, "string"),
      },
      response: RetVal("windowGlobalTarget.switchtoframe"),
    },
    listFrames: {
      request: {},
      response: RetVal("windowGlobalTarget.listframes"),
    },
    listWorkers: {
      request: {},
      response: RetVal("windowGlobalTarget.workers"),
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
      state: Option(0, "string"),
      isFrameSwitching: Option(0, "boolean"),
    },
    frameUpdate: {
      type: "frameUpdate",
      frames: Option(0, "nullable:array:windowGlobalTarget.window"),
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

const windowGlobalTargetSpec = generateActorSpec(
  windowGlobalTargetSpecPrototype
);

exports.windowGlobalTargetSpecPrototype = windowGlobalTargetSpecPrototype;
exports.windowGlobalTargetSpec = windowGlobalTargetSpec;
