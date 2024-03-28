/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { BrowserLoader } = ChromeUtils.importESModule(
  "resource://devtools/shared/loader/browser-loader.sys.mjs"
);
const require = BrowserLoader({
  baseURI: "resource://devtools/client/application/",
  window,
}).require;

const {
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  render,
  unmountComponentAtNode,
} = require("resource://devtools/client/shared/vendor/react-dom.js");
const Provider = createFactory(
  require("resource://devtools/client/shared/vendor/react-redux.js").Provider
);
const {
  bindActionCreators,
} = require("resource://devtools/client/shared/vendor/redux.js");
const {
  l10n,
} = require("resource://devtools/client/application/src/modules/l10n.js");

const {
  configureStore,
} = require("resource://devtools/client/application/src/create-store.js");
const actions = require("resource://devtools/client/application/src/actions/index.js");

const {
  WorkersListener,
} = require("resource://devtools/client/shared/workers-listener.js");

const {
  services,
} = require("resource://devtools/client/application/src/modules/application-services.js");

const App = createFactory(
  require("resource://devtools/client/application/src/components/App.js")
);

const {
  safeAsyncMethod,
} = require("resource://devtools/shared/async-utils.js");

/**
 * Global Application object in this panel. This object is expected by panel.js and is
 * called to start the UI for the panel.
 */
window.Application = {
  async bootstrap({ toolbox, commands }) {
    // bind event handlers to `this`
    this.updateDomain = this.updateDomain.bind(this);

    // wrap updateWorkers to swallow rejections occurring after destroy
    this.safeUpdateWorkers = safeAsyncMethod(
      () => this.updateWorkers(),
      () => this._destroyed
    );

    this.toolbox = toolbox;
    this._commands = commands;
    this.client = commands.client;

    this.store = configureStore(toolbox.telemetry);
    this.actions = bindActionCreators(actions, this.store.dispatch);

    services.init(this.toolbox);
    await l10n.init(["devtools/client/application.ftl"]);

    await this.updateWorkers();
    this.workersListener = new WorkersListener(this.client.mainRoot);
    this.workersListener.addListener(this.safeUpdateWorkers);

    const deviceFront = await this.client.mainRoot.getFront("device");
    const { canDebugServiceWorkers } = await deviceFront.getDescription();
    this.actions.updateCanDebugWorkers(
      canDebugServiceWorkers && services.features.doesDebuggerSupportWorkers
    );

    this.onResourceAvailable = this.onResourceAvailable.bind(this);
    await this._commands.resourceCommand.watchResources(
      [this._commands.resourceCommand.TYPES.DOCUMENT_EVENT],
      {
        onAvailable: this.onResourceAvailable,
      }
    );

    // Render the root Application component.
    this.mount = document.querySelector("#mount");
    const app = App({
      client: this.client,
      fluentBundles: l10n.getBundles(),
    });
    render(Provider({ store: this.store }, app), this.mount);
  },

  async updateWorkers() {
    const registrationsWithWorkers =
      await this.client.mainRoot.listAllServiceWorkers();
    this.actions.updateWorkers(registrationsWithWorkers);
  },

  updateDomain() {
    this.actions.updateDomain(this.toolbox.target.url);
  },

  handleOnNavigate() {
    this.updateDomain();
    this.actions.resetManifest();
  },

  onResourceAvailable(resources) {
    // Only consider top level document, and ignore remote iframes top document
    const hasDocumentDomComplete = resources.some(
      resource =>
        resource.resourceType ===
          this._commands.resourceCommand.TYPES.DOCUMENT_EVENT &&
        resource.name === "dom-complete" &&
        resource.targetFront.isTopLevel
    );
    if (hasDocumentDomComplete) {
      this.handleOnNavigate(); // update domain and manifest for the new target
    }
  },

  destroy() {
    this.workersListener.removeListener();

    this._commands.resourceCommand.unwatchResources(
      [this._commands.resourceCommand.TYPES.DOCUMENT_EVENT],
      { onAvailable: this.onResourceAvailable }
    );

    unmountComponentAtNode(this.mount);
    this.mount = null;
    this.toolbox = null;
    this.client = null;
    this._commands = null;
    this.workersListener = null;
    this._destroyed = true;
  },
};
