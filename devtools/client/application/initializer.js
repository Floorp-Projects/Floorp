/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { BrowserLoader } = ChromeUtils.import(
  "resource://devtools/client/shared/browser-loader.js"
);
const require = BrowserLoader({
  baseURI: "resource://devtools/client/application/",
  window,
}).require;

const { createFactory } = require("devtools/client/shared/vendor/react");
const {
  render,
  unmountComponentAtNode,
} = require("devtools/client/shared/vendor/react-dom");
const Provider = createFactory(
  require("devtools/client/shared/vendor/react-redux").Provider
);
const { bindActionCreators } = require("devtools/client/shared/vendor/redux");
const { l10n } = require("./src/modules/l10n");

const { configureStore } = require("./src/create-store");
const actions = require("./src/actions/index");

const { WorkersListener } = require("devtools/client/shared/workers-listener");

const {
  addDebugServiceWorkersListener,
  canDebugServiceWorkers,
  removeDebugServiceWorkersListener,
} = require("devtools/shared/service-workers-debug-helper");

const { services } = require("./src/modules/services");

const App = createFactory(require("./src/components/App"));

/**
 * Global Application object in this panel. This object is expected by panel.js and is
 * called to start the UI for the panel.
 */
window.Application = {
  async bootstrap({ toolbox, panel }) {
    this.updateWorkers = this.updateWorkers.bind(this);
    this.updateDomain = this.updateDomain.bind(this);
    this.updateCanDebugWorkers = this.updateCanDebugWorkers.bind(this);

    this.mount = document.querySelector("#mount");
    this.toolbox = toolbox;
    this.client = toolbox.target.client;

    this.store = configureStore();
    this.actions = bindActionCreators(actions, this.store.dispatch);
    this.serviceWorkerRegistrationFronts = [];

    services.init(this.toolbox);

    this.workersListener = new WorkersListener(this.client.mainRoot);
    this.workersListener.addListener(this.updateWorkers);
    this.toolbox.target.on("navigate", this.updateDomain);
    addDebugServiceWorkersListener(this.updateCanDebugWorkers);

    // start up updates for the initial state
    this.updateDomain();
    this.updateCanDebugWorkers();
    await this.updateWorkers();

    await l10n.init(["devtools/application.ftl"]);

    // Render the root Application component.
    const app = App({
      client: this.client,
      fluentBundles: l10n.getBundles(),
    });
    render(Provider({ store: this.store }, app), this.mount);
  },

  async updateWorkers() {
    const { service } = await this.client.mainRoot.listAllWorkers();
    this.actions.updateWorkers(service);
  },

  updateDomain() {
    this.actions.updateDomain(this.toolbox.target.url);
  },

  updateCanDebugWorkers() {
    const canDebugWorkers = canDebugServiceWorkers();
    this.actions.updateCanDebugWorkers(canDebugWorkers);
  },

  destroy() {
    this.workersListener.removeListener();
    this.toolbox.target.off("navigate", this.updateDomain);
    removeDebugServiceWorkersListener(this.updateCanDebugWorkers);

    unmountComponentAtNode(this.mount);
    this.mount = null;
    this.toolbox = null;
    this.client = null;
  },
};
