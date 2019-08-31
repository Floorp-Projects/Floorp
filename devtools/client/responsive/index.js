/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { BrowserLoader } = ChromeUtils.import(
  "resource://devtools/client/shared/browser-loader.js"
);
const { require } = BrowserLoader({
  baseURI: "resource://devtools/client/responsive/",
  window,
});
const Telemetry = require("devtools/client/shared/telemetry");

const {
  createFactory,
  createElement,
} = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const message = require("./utils/message");
const App = createFactory(require("./components/App"));
const Store = require("./store");
const { loadDevices, restoreDeviceState } = require("./actions/devices");
const {
  addViewport,
  resizeViewport,
  zoomViewport,
} = require("./actions/viewports");
const { changeDisplayPixelRatio } = require("./actions/ui");

// Exposed for use by tests
window.require = require;

const bootstrap = {
  telemetry: new Telemetry(),

  store: null,

  async init() {
    // responsive is not connected with a toolbox so we pass -1 as the
    // toolbox session id.
    this.telemetry.toolOpened("responsive", -1, this);

    const store = (this.store = Store());
    const provider = createElement(Provider, { store }, App());
    ReactDOM.render(provider, document.querySelector("#root"));
    message.post(window, "init:done");
  },

  destroy() {
    this.store = null;

    // responsive is not connected with a toolbox so we pass -1 as the
    // toolbox session id.
    this.telemetry.toolClosed("responsive", -1, this);
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
  bootstrap.store.dispatch(loadDevices()).then(() => {
    bootstrap.dispatch(restoreDeviceState());
  });
});

window.addEventListener(
  "unload",
  function() {
    bootstrap.destroy();
  },
  { once: true }
);

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
  const dpr = window.devicePixelRatio;
  const mql = window.matchMedia(`(resolution: ${dpr}dppx)`);

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
window.addInitialViewport = ({ uri, userContextId }) => {
  try {
    onDevicePixelRatioChange();
    bootstrap.dispatch(changeDisplayPixelRatio(window.devicePixelRatio));
    bootstrap.dispatch(addViewport(userContextId));
  } catch (e) {
    console.error(e);
  }
};

/**
 * Called by manager.js when tests want to check the viewport size.
 */
window.getViewportSize = () => {
  const { viewports } = bootstrap.store.getState();
  if (!viewports.length) {
    return null;
  }

  const { width, height } = viewports[0];
  return { width, height };
};

/**
 * Called by manager.js to set viewport size from tests, etc.
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
  const browser = document.querySelector("iframe.browser");
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

/**
 * Called by manager.js to zoom the viewport.
 */
window.setViewportZoom = zoom => {
  try {
    bootstrap.dispatch(zoomViewport(0, zoom));
  } catch (e) {
    console.error(e);
  }
};
