/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported Netmonitor */

"use strict";

const { BrowserLoader } = Components.utils.import("resource://devtools/client/shared/browser-loader.js", {});

var Netmonitor = {
  bootstrap: ({ tabTarget, toolbox }) => {
    const require = window.windowRequire = BrowserLoader({
      baseURI: "resource://devtools/client/netmonitor/",
      window,
      commonLibRequire: toolbox.browserRequire,
    }).require;

    const EventEmitter = require("devtools/shared/event-emitter");
    const { createFactory } = require("devtools/client/shared/vendor/react");
    const { render } = require("devtools/client/shared/vendor/react-dom");
    const Provider = createFactory(require("devtools/client/shared/vendor/react-redux").Provider);
    const { configureStore } = require("./store");
    const store = window.gStore = configureStore();
    const { NetMonitorController } = require("./netmonitor-controller");
    NetMonitorController.toolbox = toolbox;
    NetMonitorController._target = toolbox.target;
    this.NetMonitorController = NetMonitorController;

    // Components
    const NetworkMonitor = createFactory(require("./components/network-monitor"));

    // Inject EventEmitter into netmonitor window.
    EventEmitter.decorate(window);

    this.root = document.querySelector(".root");

    render(Provider({ store }, NetworkMonitor()), this.root);

    return NetMonitorController.startupNetMonitor();
  },

  destroy: () => {
    const require = window.windowRequire;
    const { unmountComponentAtNode } = require("devtools/client/shared/vendor/react-dom");

    unmountComponentAtNode(this.root);

    return this.NetMonitorController.shutdownNetMonitor();
  }
};
