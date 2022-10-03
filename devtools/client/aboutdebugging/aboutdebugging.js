/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  bindActionCreators,
} = require("resource://devtools/client/shared/vendor/redux.js");
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

const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const LocalizationProvider = createFactory(FluentReact.LocalizationProvider);

const actions = require("resource://devtools/client/aboutdebugging/src/actions/index.js");
const {
  configureStore,
} = require("resource://devtools/client/aboutdebugging/src/create-store.js");
const {
  setDebugTargetCollapsibilities,
} = require("resource://devtools/client/aboutdebugging/src/modules/debug-target-collapsibilities.js");

const {
  l10n,
} = require("resource://devtools/client/aboutdebugging/src/modules/l10n.js");

const {
  addNetworkLocationsObserver,
  getNetworkLocations,
  removeNetworkLocationsObserver,
} = require("resource://devtools/client/aboutdebugging/src/modules/network-locations.js");
const {
  addUSBRuntimesObserver,
  getUSBRuntimes,
  removeUSBRuntimesObserver,
} = require("resource://devtools/client/aboutdebugging/src/modules/usb-runtimes.js");

loader.lazyRequireGetter(
  this,
  "adb",
  "resource://devtools/client/shared/remote-debugging/adb/adb.js",
  true
);
loader.lazyRequireGetter(
  this,
  "adbAddon",
  "resource://devtools/client/shared/remote-debugging/adb/adb-addon.js",
  true
);
loader.lazyRequireGetter(
  this,
  "adbProcess",
  "resource://devtools/client/shared/remote-debugging/adb/adb-process.js",
  true
);

const Router = createFactory(
  require("resource://devtools/client/shared/vendor/react-router-dom.js")
    .HashRouter
);
const App = createFactory(
  require("resource://devtools/client/aboutdebugging/src/components/App.js")
);

const AboutDebugging = {
  async init() {
    const direction = Services.locale.isAppLocaleRTL ? "rtl" : "ltr";
    document.documentElement.setAttribute("dir", direction);

    this.onAdbAddonUpdated = this.onAdbAddonUpdated.bind(this);
    this.onAdbProcessReady = this.onAdbProcessReady.bind(this);
    this.onNetworkLocationsUpdated = this.onNetworkLocationsUpdated.bind(this);
    this.onUSBRuntimesUpdated = this.onUSBRuntimesUpdated.bind(this);

    this.store = configureStore();
    this.actions = bindActionCreators(actions, this.store.dispatch);

    const width = this.getRoundedViewportWidth();
    this.actions.recordTelemetryEvent("open_adbg", { width });

    await l10n.init([
      "branding/brand.ftl",
      "devtools/client/aboutdebugging.ftl",
    ]);

    this.actions.createThisFirefoxRuntime();

    // Listen to Network locations updates and retrieve the initial list of locations.
    addNetworkLocationsObserver(this.onNetworkLocationsUpdated);
    await this.onNetworkLocationsUpdated();

    // Listen to USB runtime updates and retrieve the initial list of runtimes.

    // If ADB is already started, wait for the initial runtime list to be able to restore
    // already connected runtimes.
    const isProcessStarted = await adb.isProcessStarted();
    const onAdbRuntimesReady = isProcessStarted
      ? adb.once("runtime-list-ready")
      : null;
    addUSBRuntimesObserver(this.onUSBRuntimesUpdated);
    await onAdbRuntimesReady;

    await this.onUSBRuntimesUpdated();

    render(
      Provider(
        {
          store: this.store,
        },
        LocalizationProvider(
          { bundles: l10n.getBundles() },
          Router({}, App({}))
        )
      ),
      this.mount
    );

    adbAddon.on("update", this.onAdbAddonUpdated);
    this.onAdbAddonUpdated();
    adbProcess.on("adb-ready", this.onAdbProcessReady);
    // get the initial status of adb process, in case it's already started
    this.onAdbProcessReady();
  },

  onAdbAddonUpdated() {
    this.actions.updateAdbAddonStatus(adbAddon.status);
  },

  onAdbProcessReady() {
    this.actions.updateAdbReady(adbProcess.ready);
  },

  onNetworkLocationsUpdated() {
    return this.actions.updateNetworkLocations(getNetworkLocations());
  },

  async onUSBRuntimesUpdated() {
    const runtimes = await getUSBRuntimes();
    return this.actions.updateUSBRuntimes(runtimes);
  },

  async destroy() {
    const width = this.getRoundedViewportWidth();
    this.actions.recordTelemetryEvent("close_adbg", { width });

    const state = this.store.getState();
    const currentRuntimeId = state.runtimes.selectedRuntimeId;
    if (currentRuntimeId) {
      await this.actions.unwatchRuntime(currentRuntimeId);
    }

    // Remove all client listeners.
    this.actions.removeRuntimeListeners();

    removeNetworkLocationsObserver(this.onNetworkLocationsUpdated);
    removeUSBRuntimesObserver(this.onUSBRuntimesUpdated);
    adbAddon.off("update", this.onAdbAddonUpdated);
    adbProcess.off("adb-ready", this.onAdbProcessReady);
    setDebugTargetCollapsibilities(state.ui.debugTargetCollapsibilities);
    unmountComponentAtNode(this.mount);
  },

  get mount() {
    return document.getElementById("mount");
  },

  /**
   * Computed viewport width, rounded at 50px. Used for telemetry events.
   */
  getRoundedViewportWidth() {
    return Math.ceil(window.outerWidth / 50) * 50;
  },
};

window.addEventListener(
  "DOMContentLoaded",
  () => {
    AboutDebugging.init();
  },
  { once: true }
);

window.addEventListener(
  "unload",
  () => {
    AboutDebugging.destroy();
  },
  { once: true }
);

// Expose AboutDebugging to tests so that they can access to the store.
window.AboutDebugging = AboutDebugging;
