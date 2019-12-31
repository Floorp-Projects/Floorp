/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check
/**
 * @typedef {import("../@types/perf").InitializeStoreValues} InitializeStoreValues
 * @typedef {import("../@types/perf").PopupWindow} PopupWindow
 */
"use strict";

/**
 * This file initializes the profiler popup UI. It is in charge of initializing
 * the browser specific environment, and then passing those requirements into
 * the UI. The popup is enabled by toggle the following in the browser menu:
 *
 * Tools -> Web Developer -> Enable Profiler Toolbar Icon
 */

{
  // Create the browser loader, but take care not to conflict with
  // TypeScript. See devtools/client/performance-new/typescript.md and
  // the section on "Do not overload require" for more information.

  const { BrowserLoader } = ChromeUtils.import(
    "resource://devtools/client/shared/browser-loader.js"
  );
  const browserLoader = BrowserLoader({
    baseURI: "resource://devtools/client/performance-new/popup",
    window,
  });

  /**
   * @type {any} - Coerce the current scope into an `any`, and assign the
   *     loaders to the scope. They can then be used freely below.
   */
  const scope = this;
  scope.require = browserLoader.require;
  scope.loader = browserLoader.loader;
}

/**
 * The background.jsm.js manages the profiler state, and can be loaded multiple time
 * for various components. This pop-up needs a copy, and it is also used by the
 * profiler shortcuts. In order to do this, the background code needs to live in a
 * JSM module, that can be shared with the DevTools keyboard shortcut manager.
 */
const {
  getRecordingPreferencesFromBrowser,
  setRecordingPreferencesOnBrowser,
  getSymbolsFromThisBrowser,
} =
  /** @type {import("resource://devtools/client/performance-new/popup/background.jsm.js")} */
  (ChromeUtils.import(
    "resource://devtools/client/performance-new/popup/background.jsm.js"
  ));

const { receiveProfile } = require("devtools/client/performance-new/browser");

const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const React = require("devtools/client/shared/vendor/react");
const DevToolsAndPopup = React.createFactory(
  require("devtools/client/performance-new/components/DevToolsAndPopup")
);
const ProfilerEventHandling = React.createFactory(
  require("devtools/client/performance-new/components/ProfilerEventHandling")
);
const createStore = require("devtools/client/shared/redux/create-store");
const reducers = require("devtools/client/performance-new/store/reducers");
const actions = require("devtools/client/performance-new/store/actions");
const { Provider } = require("devtools/client/shared/vendor/react-redux");
const {
  ActorReadyGeckoProfilerInterface,
} = require("devtools/shared/performance-new/gecko-profiler-interface");

/* Force one of our two themes depending on what theme the browser is
 * currently using. This might be different from the selected theme in
 * the devtools panel. By forcing a theme here, we're unaffected by
 * the devtools setting when we show the popup.
 */
document.documentElement.setAttribute(
  "force-theme",
  window.gIsDarkMode ? "dark" : "light"
);

document.addEventListener("DOMContentLoaded", () => {
  gInit();
});

/**
 * Initialize the panel by creating a redux store, and render the root component.
 */
async function gInit() {
  const store = createStore(reducers);
  const perfFrontInterface = new ActorReadyGeckoProfilerInterface();
  const supportedFeatures = await perfFrontInterface.getSupportedFeatures();

  // Do some initialization, especially with privileged things that are part of the
  // the browser.
  store.dispatch(
    actions.initializeStore({
      perfFront: perfFrontInterface,
      receiveProfile,
      supportedFeatures,
      // Get the preferences from the current browser
      recordingPreferences: getRecordingPreferencesFromBrowser(),
      // In the popup, the preferences are stored directly on the current browser.
      setRecordingPreferences: setRecordingPreferencesOnBrowser,
      // The popup doesn't need to support remote symbol tables from the debuggee.
      // Only get the symbols from this browser.
      getSymbolTableGetter: () => getSymbolsFromThisBrowser,
      pageContext: "popup",
    })
  );

  ReactDOM.render(
    React.createElement(
      Provider,
      { store },
      React.createElement(
        React.Fragment,
        null,
        ProfilerEventHandling(),
        DevToolsAndPopup()
      )
    ),
    document.querySelector("#root")
  );

  window.addEventListener("unload", function() {
    // The perf front interface needs to be unloaded in order to remove event handlers.
    // Not doing so leads to leaks.
    perfFrontInterface.destroy();
  });

  resizeWindow();
}

function resizeWindow() {
  window.requestAnimationFrame(() => {
    // Coerce the window object into the PopupWindow interface.
    /** @type {any} */
    const anyWindow = window;
    /** @type {PopupWindow} */
    const { gResizePopup } = anyWindow;
    if (gResizePopup) {
      gResizePopup(document.body.clientHeight);
    }
  });
}
