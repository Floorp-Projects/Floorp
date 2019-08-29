/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  types,
  generateActorSpec,
  RetVal,
  Option,
} = require("devtools/shared/protocol");

types.addDictType("browsingContextTarget.attach", {
  type: "string",
  threadActor: "number",
  cacheDisabled: "boolean",
  javascriptEnabled: "boolean",
  traits: "json",
});

types.addDictType("browsingContextTarget.detach", {
  error: "nullable:string",
  type: "nullable:string",
});

types.addDictType("browsingContextTarget.switchtoframe", {
  error: "nullable:string",
  message: "nullable:string",
});

types.addDictType("browsingContextTarget.listframes", {
  frames: "array:browsingContextTarget.window",
});

types.addDictType("browsingContextTarget.listRemoteFrames", {
  frames: "array:frameDescriptor",
});

types.addDictType("browsingContextTarget.window", {
  id: "string",
  parentID: "nullable:string",
  url: "nullable:string", // should be present if not destroying
  title: "nullable:string", // should be present if not destroying
  destroy: "nullable:boolean", // not present if not destroying
});

types.addDictType("browsingContextTarget.workers", {
  error: "nullable:string",
  workers: "nullable:array:workerTarget",
});

types.addDictType("browsingContextTarget.reload", {
  force: "boolean",
});

types.addDictType("browsingContextTarget.reconfigure", {
  javascriptEnabled: "nullable:boolean",
  cacheDisabled: "nullable:boolean",
  serviceWorkersTestingEnabled: "nullable:boolean",
  performReload: "nullable:boolean",
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
      response: RetVal("browsingContextTarget.detach"),
    },
    ensureCSSErrorReportingEnabled: {
      request: {},
      response: {},
    },
    focus: {
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
    listRemoteFrames: {
      request: {},
      response: RetVal("browsingContextTarget.listRemoteFrames"),
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
    tabDetached: {
      type: "tabDetached",
      // This is to make browser_dbg_navigation.js to work as it expect to
      // see a packet object when listening for tabDetached
      from: Option(0, "string"),
    },
    workerListChanged: {
      type: "workerListChanged",
    },

    // The thread actor is no longer emitting newSource event in the name of the target
    // actor (bug 1269919), but as we may still connect to older servers which still do,
    // we have to keep it being mentioned here. Otherwise the event is considered as a
    // response to a request and confuses the packet ordering.
    // We can remove that once FF66 is no longer supported.
    newSource: {
      type: "newSource",
    },
  },
};

const browsingContextTargetSpec = generateActorSpec(
  browsingContextTargetSpecPrototype
);

exports.browsingContextTargetSpecPrototype = browsingContextTargetSpecPrototype;
exports.browsingContextTargetSpec = browsingContextTargetSpec;
