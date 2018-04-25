/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { BrowserLoader } = ChromeUtils.import("resource://devtools/client/shared/browser-loader.js", {});
const require = BrowserLoader({
  baseURI: "resource://devtools/client/application/",
  window,
}).require;

const { createFactory } = require("devtools/client/shared/vendor/react");
const { render, unmountComponentAtNode } = require("devtools/client/shared/vendor/react-dom");
const Provider = createFactory(require("devtools/client/shared/vendor/react-redux").Provider);
const { bindActionCreators } = require("devtools/client/shared/vendor/redux");

const { configureStore } = require("./src/create-store");
const actions = require("./src/actions/index");

const App = createFactory(require("./src/components/App"));

/**
 * Global Application object in this panel. This object is expected by panel.js and is
 * called to start the UI for the panel.
 */
window.Application = {
  async bootstrap({ toolbox, panel }) {
    this.updateWorkers = this.updateWorkers.bind(this);
    this.updateDomain = this.updateDomain.bind(this);

    this.mount = document.querySelector("#mount");
    this.toolbox = toolbox;
    this.client = toolbox.target.client;

    this.store = configureStore();
    this.actions = bindActionCreators(actions, this.store.dispatch);

    const serviceContainer = {
      openWebLink(url) {
        let win = toolbox.doc.defaultView.top;
        win.openWebLinkIn(url, "tab", { relatedToCurrent: true });
      },

      openTrustedLink(url) {
        let win = toolbox.doc.defaultView.top;
        win.openTrustedLinkIn(url, "tab", { relatedToCurrent: true });
      },

      selectTool(toolId) {
        return toolbox.selectTool(toolId);
      }
    };

    // Render the root Application component.
    const app = App({ client: this.client, serviceContainer });
    render(Provider({ store: this.store }, app), this.mount);

    this.client.addListener("workerListChanged", this.updateWorkers);
    this.client.addListener("serviceWorkerRegistrationListChanged", this.updateWorkers);
    this.client.addListener("registration-changed", this.updateWorkers);
    this.client.addListener("processListChanged", this.updateWorkers);
    this.toolbox.target.on("navigate", this.updateDomain);

    this.updateDomain();
    await this.updateWorkers();
  },

  async updateWorkers() {
    let { service } = await this.client.mainRoot.listAllWorkers();
    this.actions.updateWorkers(service);
  },

  updateDomain() {
    this.actions.updateDomain(this.toolbox.target.url);
  },

  destroy() {
    this.client.removeListener("workerListChanged", this.updateWorkers);
    this.client.removeListener("serviceWorkerRegistrationListChanged",
      this.updateWorkers);
    this.client.removeListener("registration-changed", this.updateWorkers);
    this.client.removeListener("processListChanged", this.updateWorkers);

    this.toolbox.target.off("navigate", this.updateDomain);

    unmountComponentAtNode(this.mount);
    this.mount = null;
    this.toolbox = null;
    this.client = null;
  },
};
