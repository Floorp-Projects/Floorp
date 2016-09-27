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
  window
});
const { Task } = require("devtools/shared/task");
const Telemetry = require("devtools/client/shared/telemetry");
const { loadSheet } = require("sdk/stylesheet/utils");

const { createFactory, createElement } =
  require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const message = require("./utils/message");
const App = createFactory(require("./app"));
const Store = require("./store");
const { changeLocation } = require("./actions/location");
const { addViewport, resizeViewport } = require("./actions/viewports");
const { loadDevices } = require("./actions/devices");

let bootstrap = {

  telemetry: new Telemetry(),

  store: null,

  init: Task.async(function* () {
    // Load a special UA stylesheet to reset certain styles such as dropdown
    // lists.
    loadSheet(window,
              "resource://devtools/client/responsive.html/responsive-ua.css",
              "agent");
    this.telemetry.toolOpened("responsive");
    let store = this.store = Store();
    this.dispatch(loadDevices());
    let provider = createElement(Provider, { store }, App());
    ReactDOM.render(provider, document.querySelector("#root"));
    message.post(window, "init:done");
  }),

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

};

// manager.js sends a message to signal init
message.wait(window, "init").then(() => bootstrap.init());

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

/**
 * Called by manager.js when tests want to check the viewport size.
 */
window.getViewportSize = () => {
  let { width, height } = bootstrap.store.getState().viewports[0];
  return { width, height };
};

/**
 * Called by manager.js to set viewport size from GCLI.
 */
window.setViewportSize = (width, height) => {
  try {
    bootstrap.dispatch(resizeViewport(0, width, height));
  } catch (e) {
    console.error(e);
  }
};

/**
 * Called by manager.js to access the viewport's browser, either for testing
 * purposes or to reload it when touch simulation is enabled.
 * A messageManager getter is added on the object to provide an easy access
 * to the message manager without pulling the frame loader.
 */
window.getViewportBrowser = () => {
  let browser = document.querySelector("iframe.browser");
  if (!browser.messageManager) {
    Object.defineProperty(browser, "messageManager", {
      get() {
        return this.frameLoader.messageManager;
      },
      configurable: true,
      enumerable: true,
    });
  }
  return browser;
};
