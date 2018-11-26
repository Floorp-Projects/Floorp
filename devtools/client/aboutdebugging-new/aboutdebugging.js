/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

const { bindActionCreators } = require("devtools/client/shared/vendor/redux");
const { createFactory } =
  require("devtools/client/shared/vendor/react");
const { render, unmountComponentAtNode } =
  require("devtools/client/shared/vendor/react-dom");
const Provider =
  createFactory(require("devtools/client/shared/vendor/react-redux").Provider);

const actions = require("./src/actions/index");
const { configureStore } = require("./src/create-store");
const {
  setDebugTargetCollapsibilities,
} = require("./src/modules/debug-target-collapsibilities");

const { l10n } = require("./src/modules/l10n");

const {
  addNetworkLocationsObserver,
  getNetworkLocations,
  removeNetworkLocationsObserver,
} = require("./src/modules/network-locations");
const {
  addUSBRuntimesObserver,
  disableUSBRuntimes,
  enableUSBRuntimes,
  getUSBRuntimes,
  removeUSBRuntimesObserver,
} = require("./src/modules/usb-runtimes");

loader.lazyRequireGetter(this, "adbAddon", "devtools/shared/adb/adb-addon", true);

const App = createFactory(require("./src/components/App"));

const { PAGES, RUNTIMES } = require("./src/constants");

const AboutDebugging = {
  async init() {
    if (!Services.prefs.getBoolPref("devtools.enabled", true)) {
      // If DevTools are disabled, navigate to about:devtools.
      window.location = "about:devtools?reason=AboutDebugging";
      return;
    }

    this.onAdbAddonUpdated = this.onAdbAddonUpdated.bind(this);
    this.onNetworkLocationsUpdated = this.onNetworkLocationsUpdated.bind(this);
    this.onUSBRuntimesUpdated = this.onUSBRuntimesUpdated.bind(this);

    this.store = configureStore();
    this.actions = bindActionCreators(actions, this.store.dispatch);

    await l10n.init();

    render(
      Provider({ store: this.store }, App({ fluentBundles: l10n.getBundles() })),
      this.mount
    );

    this.actions.selectPage(PAGES.THIS_FIREFOX, RUNTIMES.THIS_FIREFOX);
    this.actions.updateNetworkLocations(getNetworkLocations());

    addNetworkLocationsObserver(this.onNetworkLocationsUpdated);
    addUSBRuntimesObserver(this.onUSBRuntimesUpdated);
    await enableUSBRuntimes();

    adbAddon.on("update", this.onAdbAddonUpdated);
    this.onAdbAddonUpdated();

    // Remove deprecated remote debugging extensions.
    await adbAddon.uninstallUnsupportedExtensions();
  },

  onAdbAddonUpdated() {
    this.actions.updateAdbAddonStatus(adbAddon.status);
  },

  onNetworkLocationsUpdated() {
    this.actions.updateNetworkLocations(getNetworkLocations());
  },

  onUSBRuntimesUpdated() {
    this.actions.updateUSBRuntimes(getUSBRuntimes());
  },

  async destroy() {
    const state = this.store.getState();

    l10n.destroy();

    const currentRuntimeId = state.runtimes.selectedRuntimeId;
    if (currentRuntimeId) {
      await this.actions.unwatchRuntime(currentRuntimeId);
    }

    // Remove all client listeners.
    this.actions.removeRuntimeListeners();

    removeNetworkLocationsObserver(this.onNetworkLocationsUpdated);
    removeUSBRuntimesObserver(this.onUSBRuntimesUpdated);
    disableUSBRuntimes();
    adbAddon.off("update", this.onAdbAddonUpdated);
    setDebugTargetCollapsibilities(state.ui.debugTargetCollapsibilities);
    unmountComponentAtNode(this.mount);
  },

  get mount() {
    return document.getElementById("mount");
  },
};

window.addEventListener("DOMContentLoaded", () => {
  AboutDebugging.init();
}, { once: true });

window.addEventListener("unload", () => {
  AboutDebugging.destroy();
}, {once: true});

// Expose AboutDebugging to tests so that they can access to the store.
window.AboutDebugging = AboutDebugging;
