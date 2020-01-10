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
const { l10n } = require("devtools/client/application/src/modules/l10n");

const {
  configureStore,
} = require("devtools/client/application/src/create-store");
const actions = require("devtools/client/application/src/actions/index");

const { WorkersListener } = require("devtools/client/shared/workers-listener");

const {
  services,
} = require("devtools/client/application/src/modules/application-services");

const App = createFactory(
  require("devtools/client/application/src/components/App")
);

/**
 * Global Application object in this panel. This object is expected by panel.js and is
 * called to start the UI for the panel.
 */
window.Application = {
  async bootstrap({ toolbox, panel }) {
    this.handleOnNavigate = this.handleOnNavigate.bind(this);
    this.updateWorkers = this.updateWorkers.bind(this);
    this.updateDomain = this.updateDomain.bind(this);
    this.updateCanDebugWorkers = this.updateCanDebugWorkers.bind(this);

    this.mount = document.querySelector("#mount");
    this.toolbox = toolbox;
    this.client = toolbox.target.client;

    this.store = configureStore();
    this.actions = bindActionCreators(actions, this.store.dispatch);

    services.init(this.toolbox);

    this.deviceFront = await this.client.mainRoot.getFront("device");

    this.workersListener = new WorkersListener(this.client.mainRoot);
    this.workersListener.addListener(this.updateWorkers);
    this.toolbox.target.on("navigate", this.handleOnNavigate);

    if (this.deviceFront) {
      this.canDebugWorkersListener = this.deviceFront.on(
        "can-debug-sw-updated",
        this.updateCanDebugWorkers
      );
    }

    // start up updates for the initial state
    this.updateDomain();
    await this.updateCanDebugWorkers();
    await this.updateWorkers();

    await l10n.init(["devtools/client/application.ftl"]);

    // Render the root Application component.
    const app = App({
      client: this.client,
      fluentBundles: l10n.getBundles(),
    });
    render(Provider({ store: this.store }, app), this.mount);
  },

  handleOnNavigate() {
    this.updateDomain();
    this.actions.resetManifest();
  },

  async updateWorkers() {
    const { service } = await this.client.mainRoot.listAllWorkers();
    // filter out workers that don't have an URL or a scope
    // TODO: Bug 1595138 investigate why we lack those properties
    const workers = service.filter(x => x.url && x.scope);

    this.actions.updateWorkers(workers);
  },

  updateDomain() {
    this.actions.updateDomain(this.toolbox.target.url);
  },

  async updateCanDebugWorkers() {
    const canDebugWorkers = this.deviceFront
      ? (await this.deviceFront.getDescription()).canDebugServiceWorkers
      : false;

    this.actions.updateCanDebugWorkers(
      canDebugWorkers && services.features.doesDebuggerSupportWorkers
    );
  },

  destroy() {
    this.workersListener.removeListener();
    if (this.deviceFront) {
      this.deviceFront.off("can-debug-sw-updated", this.updateCanDebugWorkers);
    }
    this.toolbox.target.off("navigate", this.updateDomain);

    unmountComponentAtNode(this.mount);
    this.mount = null;
    this.toolbox = null;
    this.client = null;
  },
};
