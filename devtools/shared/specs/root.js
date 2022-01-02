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
  workers: "array:workerDescriptor",
});
types.addDictType("root.listServiceWorkerRegistrations", {
  registrations: "array:serviceWorkerRegistration",
});

const rootSpecPrototype = {
  typeName: "root",

  methods: {
    getRoot: {
      request: {},
      response: RetVal("json"),
    },

    listTabs: {
      request: {},
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
