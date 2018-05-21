/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { BrowserLoader } =
  ChromeUtils.import("resource://devtools/client/shared/browser-loader.js", {});
const { require } = BrowserLoader({
  baseURI: "resource://devtools/client/responsive.html/",
  window
});
const Telemetry = require("devtools/client/shared/telemetry");
const { loadAgentSheet } = require("./utils/css");

const { createFactory, createElement } =
  require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const message = require("./utils/message");
const App = createFactory(require("./app"));
const Store = require("./store");
const { loadDevices } = require("./actions/devices");
const { changeDisplayPixelRatio } = require("./actions/display-pixel-ratio");
const { changeLocation } = require("./actions/location");
const { loadReloadConditions } = require("./actions/reload-conditions");
const { addViewport, resizeViewport } = require("./actions/viewports");

// Exposed for use by tests
window.require = require;

let bootstrap = {

  telemetry: new Telemetry(),

  store: null,

  async init() {
    // Load a special UA stylesheet to reset certain styles such as dropdown
    // lists.
    loadAgentSheet(
      window,
      "resource://devtools/client/responsive.html/responsive-ua.css"
    );

    this.telemetry.toolOpened("responsive");

    let store = this.store = Store();
    let provider = createElement(Provider, { store }, App());
    ReactDOM.render(provider, document.querySelector("#root"));
    message.post(window, "init:done");
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

};

// manager.js sends a message to signal init
message.wait(window, "init").then(() => bootstrap.init());

// manager.js sends a message to signal init is done, which can be used for delayed
// startup work that shouldn't block initial load
message.wait(window, "post-init").then(() => {
  bootstrap.dispatch(loadDevices());
  bootstrap.dispatch(loadReloadConditions());
});

window.addEventListener("unload", function() {
  bootstrap.destroy();
}, {once: true});

// Allows quick testing of actions from the console
window.dispatch = action => bootstrap.dispatch(action);

// Expose the store on window for testing
Object.defineProperty(window, "store", {
  get: () => bootstrap.store,
  enumerable: true,
});

// Dispatch a `changeDisplayPixelRatio` action when the browser's pixel ratio is changing.
// This is usually triggered when the user changes the monitor resolution, or when the
// browser's window is dragged to a different display with a different pixel ratio.
// TODO: It would be better to move this watching into the actor, so that it can be
// better synchronized with any overrides that might be applied.  Also, reading a single
// value like this makes less sense with multiple viewports.
function onDevicePixelRatioChange() {
  let dpr = window.devicePixelRatio;
  let mql = window.matchMedia(`(resolution: ${dpr}dppx)`);

  function listener() {
    bootstrap.dispatch(changeDisplayPixelRatio(window.devicePixelRatio));
    mql.removeListener(listener);
    onDevicePixelRatioChange();
  }

  mql.addListener(listener);
}

/**
 * Called by manager.js to add the initial viewport based on the original page.
 */
window.addInitialViewport = contentURI => {
  try {
    onDevicePixelRatioChange();
    bootstrap.dispatch(changeLocation(contentURI));
    bootstrap.dispatch(changeDisplayPixelRatio(window.devicePixelRatio));
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
 * Called by manager.js to set viewport size from tests, GCLI, etc.
 */
window.setViewportSize = ({ width, height }) => {
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
