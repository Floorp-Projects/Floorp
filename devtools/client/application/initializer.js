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
    // bind event handlers to `this`
    this.handleOnNavigate = this.handleOnNavigate.bind(this);
    this.updateWorkers = this.updateWorkers.bind(this);
    this.updateDomain = this.updateDomain.bind(this);
    this.updateCanDebugWorkers = this.updateCanDebugWorkers.bind(this);
    this.onTargetAvailable = this.onTargetAvailable.bind(this);
    this.onTargetDestroyed = this.onTargetDestroyed.bind(this);

    this.toolbox = toolbox;
    // NOTE: the client is the same through the lifecycle of the toolbox, even
    // though we get it from toolbox.target
    this.client = toolbox.target.client;

    this.store = configureStore();
    this.actions = bindActionCreators(actions, this.store.dispatch);

    services.init(this.toolbox);
    await l10n.init(["devtools/client/application.ftl"]);

    await this.updateWorkers();
    this.workersListener = new WorkersListener(this.client.mainRoot);
    this.workersListener.addListener(this.updateWorkers);

    this.deviceFront = await this.client.mainRoot.getFront("device");
    await this.updateCanDebugWorkers();
    if (this.deviceFront) {
      this.canDebugWorkersListener = this.deviceFront.on(
        "can-debug-sw-updated",
        this.updateCanDebugWorkers
      );
    }

    // awaiting for watchTargets will return the targets that are currently
    // available, so we can have our first render with all the data ready
    await this.toolbox.targetList.watchTargets(
      [this.toolbox.targetList.TYPES.FRAME],
      this.onTargetAvailable,
      this.onTargetDestroyed
    );

    // Render the root Application component.
    this.mount = document.querySelector("#mount");
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

  setupTarget(targetFront) {
    this.handleOnNavigate(); // update domain and manifest for the new target
    targetFront.on("navigate", this.handleOnNavigate);
  },

  cleanUpTarget(targetFront) {
    targetFront.off("navigate", this.handleOnNavigate);
  },

  onTargetAvailable({ targetFront, isTopLevel }) {
    if (!isTopLevel) {
      return; // ignore target frames that are not top level for now
    }

    this.setupTarget(targetFront);
  },

  onTargetDestroyed({ targetFront, isTopLevel }) {
    if (!isTopLevel) {
      return; // ignore target frames that are not top level for now
    }

    this.cleanUpTarget(targetFront);
  },

  destroy() {
    this.workersListener.removeListener();
    if (this.deviceFront) {
      this.deviceFront.off("can-debug-sw-updated", this.updateCanDebugWorkers);
    }

    this.toolbox.targetList.unwatchTargets(
      [this.toolbox.targetList.TYPES.FRAME],
      this.onTargetAvailable,
      this.onTargetDestroyed
    );

    this.cleanUpTarget(this.toolbox.target);

    unmountComponentAtNode(this.mount);
    this.mount = null;
    this.toolbox = null;
    this.client = null;
    this.workersListener = null;
    this.deviceFront = null;
  },
};
