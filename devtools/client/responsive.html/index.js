/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { utils: Cu } = Components;
const { BrowserLoader } =
  Cu.import("resource://devtools/client/shared/browser-loader.js", {});
const { require } = BrowserLoader({
  baseURI: "resource://devtools/client/responsive.html/",
  window: this
});
const { GetDevices } = require("devtools/client/shared/devices");
const Telemetry = require("devtools/client/shared/telemetry");

const { createFactory, createElement } =
  require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const App = createFactory(require("./app"));
const Store = require("./store");
const { addDevice, addDeviceType } = require("./actions/devices");
const { changeLocation } = require("./actions/location");
const { addViewport } = require("./actions/viewports");
const { loadSheet } = require("sdk/stylesheet/utils");

let bootstrap = {

  telemetry: new Telemetry(),

  store: null,

  init() {
    // Load a special UA stylesheet to reset certain styles such as dropdown
    // lists.
    loadSheet(window,
              "resource://devtools/client/responsive.html/responsive-ua.css",
              "agent");
    this.telemetry.toolOpened("responsive");
    let store = this.store = Store();
    let app = App({
      onExit: () => window.postMessage({ type: "exit" }, "*"),
    });
    let provider = createElement(Provider, { store }, app);
    ReactDOM.render(provider, document.querySelector("#root"));
    this.initDevices();
    window.postMessage({ type: "init" }, "*");
  },

  destroy() {
    this.store = null;
    this.telemetry.toolClosed("responsive");
    this.telemetry = null;
  },

  /**
   * While most actions will be dispatched by React components, some external
   * APIs that coordinate with the larger browser UI may also have actions to
   * to dispatch.  They can do so here.
   */
  dispatch(action) {
    if (!this.store) {
      // If actions are dispatched after store is destroyed, ignore them.  This
      // can happen in tests that close the tool quickly while async tasks like
      // initDevices() below are still pending.
      return;
    }
    this.store.dispatch(action);
  },

  initDevices() {
    GetDevices().then(devices => {
      for (let type of devices.TYPES) {
        this.dispatch(addDeviceType(type));
        for (let device of devices[type]) {
          if (device.os != "fxos") {
            this.dispatch(addDevice(device, type));
          }
        }
      }
    });
  },

};

window.addEventListener("load", function onLoad() {
  window.removeEventListener("load", onLoad);
  bootstrap.init();
});

window.addEventListener("unload", function onUnload() {
  window.removeEventListener("unload", onUnload);
  bootstrap.destroy();
});

// Allows quick testing of actions from the console
window.dispatch = action => bootstrap.dispatch(action);

// Expose the store on window for testing
Object.defineProperty(window, "store", {
  get: () => bootstrap.store,
  enumerable: true,
});

/**
 * Called by manager.js to add the initial viewport based on the original page.
 */
window.addInitialViewport = contentURI => {
  try {
    bootstrap.dispatch(changeLocation(contentURI));
    bootstrap.dispatch(addViewport());
  } catch (e) {
    console.error(e);
  }
};
