/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  types,
  generateActorSpec,
  RetVal,
  Arg,
  Option,
} = require("devtools/shared/protocol");

types.addDictType("root.listWorkers", {
  workers: "array:workerTarget",
});
types.addDictType("root.listServiceWorkerRegistrations", {
  registrations: "array:serviceWorkerRegistration",
});
// TODO: This can be removed once FF77 is the release
// This is only kept to support older version. FF77+ uses watchTargets.
types.addDictType("root.listRemoteFrames", {
  frames: "array:frameDescriptor",
});
types.addPolymorphicType("root.browsingContextDescriptor", [
  "frameDescriptor",
  "processDescriptor",
]);

const rootSpecPrototype = {
  typeName: "root",

  methods: {
    getRoot: {
      request: {},
      response: RetVal("json"),
    },

    listTabs: {
      request: {
        // Backward compatibility: this is only used for FF75 or older.
        // The argument can be dropped when FF76 hits the release channel.
        favicons: Option(0, "boolean"),
      },
      response: {
        tabs: RetVal("array:tabDescriptor"),
      },
    },

    getTab: {
      request: {
        outerWindowID: Option(0, "number"),
        tabId: Option(0, "number"),
      },
      response: {
        tab: RetVal("tabDescriptor"),
      },
    },

    getWindow: {
      request: {
        outerWindowID: Option(0, "number"),
      },
      response: {
        window: RetVal("browsingContextTarget"),
      },
    },

    listAddons: {
      request: {
        iconDataURL: Option(0, "boolean"),
      },
      response: {
        addons: RetVal("array:webExtensionDescriptor"),
      },
    },

    listWorkers: {
      request: {},
      response: RetVal("root.listWorkers"),
    },

    listServiceWorkerRegistrations: {
      request: {},
      response: RetVal("root.listServiceWorkerRegistrations"),
    },

    listProcesses: {
      request: {},
      response: {
        processes: RetVal("array:processDescriptor"),
      },
    },

    getProcess: {
      request: {
        id: Arg(0, "number"),
      },
      response: {
        processDescriptor: RetVal("processDescriptor"),
      },
    },

    // TODO: This can be removed once FF77 is the release
    // This is only kept to support older version. FF77+ uses watchTargets.
    listRemoteFrames: {
      request: {
        id: Arg(0, "number"),
      },
      response: RetVal("root.listRemoteFrames"),
    },

    // Can be removed when FF77 reach release channel
    getBrowsingContextDescriptor: {
      request: {
        id: Arg(0, "number"),
      },
      response: RetVal("root.browsingContextDescriptor"),
    },

    protocolDescription: {
      request: {},
      response: RetVal("json"),
    },

    requestTypes: {
      request: {},
      response: RetVal("json"),
    },

    // Note that RootFront also implements 'echo' requests
    // that can't be described via protocol.js specs.
  },

  events: {
    tabListChanged: {
      type: "tabListChanged",
    },
    workerListChanged: {
      type: "workerListChanged",
    },
    addonListChanged: {
      type: "addonListChanged",
    },
    serviceWorkerRegistrationListChanged: {
      type: "serviceWorkerRegistrationListChanged",
    },
    processListChanged: {
      type: "processListChanged",
    },
  },
};

const rootSpec = generateActorSpec(rootSpecPrototype);

exports.rootSpecPrototype = rootSpecPrototype;
exports.rootSpec = rootSpec;
