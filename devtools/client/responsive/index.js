/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { BrowserLoader } = ChromeUtils.import(
  "resource://devtools/shared/loader/browser-loader.js"
);
const { require } = BrowserLoader({
  baseURI: "resource://devtools/client/responsive/",
  window,
});
const Telemetry = require("resource://devtools/client/shared/telemetry.js");

const {
  createFactory,
  createElement,
} = require("resource://devtools/client/shared/vendor/react.js");
const ReactDOM = require("resource://devtools/client/shared/vendor/react-dom.js");
const {
  Provider,
} = require("resource://devtools/client/shared/vendor/react-redux.js");

const message = require("resource://devtools/client/responsive/utils/message.js");
const App = createFactory(
  require("resource://devtools/client/responsive/components/App.js")
);
const Store = require("resource://devtools/client/responsive/store.js");
const {
  loadDevices,
  restoreDeviceState,
} = require("resource://devtools/client/responsive/actions/devices.js");
const {
  addViewport,
  changePixelRatio,
  removeDeviceAssociation,
  resizeViewport,
  zoomViewport,
} = require("resource://devtools/client/responsive/actions/viewports.js");
const {
  changeDisplayPixelRatio,
  changeUserAgent,
  toggleTouchSimulation,
} = require("resource://devtools/client/responsive/actions/ui.js");

// Exposed for use by tests
window.require = require;

// Tell the ResponsiveUIManager that the frame script has begun initializing.
message.post(window, "script-init");

const bootstrap = {
  telemetry: new Telemetry(),

  store: null,

  async init() {
    this.telemetry.toolOpened("responsive", this);

    const store = (this.store = Store());
    const provider = createElement(Provider, { store }, App());
    this._root = document.querySelector("#root");
    ReactDOM.render(provider, this._root);
    message.post(window, "init:done");

    this.destroy = this.destroy.bind(this);
    window.addEventListener("unload", this.destroy, { once: true });
  },

  destroy() {
    window.removeEventListener("unload", this.destroy, { once: true });

    // unmount to stop async action and renders after destroy
    ReactDOM.unmountComponentAtNode(this._root);

    this.store = null;

    this.telemetry.toolClosed("responsive", this);
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
      return Promise.resolve();
    }
    return this.store.dispatch(action);
  },
};

// manager.js sends a message to signal init
message.wait(window, "init").then(() => bootstrap.init());

// manager.js sends a message to signal init is done, which can be used for delayed
// startup work that shouldn't block initial load
message.wait(window, "post-init").then(() => {
  bootstrap.dispatch(loadDevices()).then(() => {
    bootstrap.dispatch(restoreDeviceState());
  });
});

window.destroy = () => bootstrap.destroy();
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
window.addInitialViewport = ({ userContextId }) => {
  try {
    onDevicePixelRatioChange();
    bootstrap.dispatch(changeDisplayPixelRatio(window.devicePixelRatio));
    bootstrap.dispatch(addViewport(userContextId));
  } catch (e) {
    console.error(e);
  }
};

window.getAssociatedDevice = () => {
  const { viewports } = bootstrap.store.getState();
  if (!viewports.length) {
    return null;
  }

  return viewports[0].device;
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

window.clearDeviceAssociation = () => {
  try {
    bootstrap.dispatch(removeDeviceAssociation(0));
    bootstrap.dispatch(toggleTouchSimulation(false));
    bootstrap.dispatch(changePixelRatio(0, 0));
    bootstrap.dispatch(changeUserAgent(""));
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
  if (browser && !browser.messageManager) {
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
