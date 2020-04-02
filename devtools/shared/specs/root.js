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
types.addDictType("root.listRemoteFrames", {
  frames: "array:frameDescriptor",
});
// Backward compatibility: FF74 or older servers will return the
// process descriptor as the "form" property of the response.
// Once FF75 is merged to release we can always expect `processDescriptor`
// to be defined.
types.addDictType("root.getProcess", {
  form: "nullable:processDescriptor",
  processDescriptor: "nullable:processDescriptor",
});
types.addDictType("root.listTabs", {
  // Backwards compatibility for servers FF74 and before
  // once FF75 is merged into release, we can return tabDescriptors directly.
  tabs: "array:json",
  selected: "number",
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
      response: RetVal("root.listTabs"),
    },

    getTab: {
      request: {
        outerWindowID: Option(0, "number"),
        tabId: Option(0, "number"),
      },
      response: {
        // Backwards compatibility for servers FF74 and before
        // once FF75 is merged into release, we can return the tabDescriptor directly.
        tab: RetVal("json"),
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
      response: RetVal("root.getProcess"),
    },

    listRemoteFrames: {
      request: {
        id: Arg(0, "number"),
      },
      response: RetVal("root.listRemoteFrames"),
    },

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
