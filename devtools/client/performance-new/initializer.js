/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check
/* exported gInit, gDestroy, loader */

/**
 * @typedef {import("./@types/perf").PerfFront} PerfFront
 * @typedef {import("./@types/perf").PreferenceFront} PreferenceFront
 * @typedef {import("./@types/perf").RecordingStateFromPreferences} RecordingStateFromPreferences
 */
"use strict";

{
  // Create the browser loader, but take care not to conflict with
  // TypeScript. See devtools/client/performance-new/typescript.md and
  // the section on "Do not overload require" for more information.

  const { BrowserLoader } = ChromeUtils.import(
    "resource://devtools/client/shared/browser-loader.js"
  );
  const browserLoader = BrowserLoader({
    baseURI: "resource://devtools/client/performance-new/",
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

const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const React = require("devtools/client/shared/vendor/react");
const DevToolsAndPopup = React.createFactory(
  require("devtools/client/performance-new/components/DevToolsAndPopup")
);
const ProfilerEventHandling = React.createFactory(
  require("devtools/client/performance-new/components/ProfilerEventHandling")
);
const createStore = require("devtools/client/shared/redux/create-store");
const selectors = require("devtools/client/performance-new/store/selectors");
const reducers = require("devtools/client/performance-new/store/reducers");
const actions = require("devtools/client/performance-new/store/actions");
const { Provider } = require("devtools/client/shared/vendor/react-redux");
const {
  receiveProfile,
  getRecordingPreferencesFromDebuggee,
  setRecordingPreferencesOnDebuggee,
  createMultiModalGetSymbolTableFn,
} = require("devtools/client/performance-new/browser");

const { getDefaultRecordingPreferencesForOlderFirefox } = ChromeUtils.import(
  "resource://devtools/client/performance-new/popup/background.jsm.js"
);

/**
 * This file initializes the DevTools Panel UI. It is in charge of initializing
 * the DevTools specific environment, and then passing those requirements into
 * the UI.
 */

/**
 * Initialize the panel by creating a redux store, and render the root component.
 *
 * @param {PerfFront} perfFront - The Perf actor's front. Used to start and stop recordings.
 * @param {PreferenceFront} preferenceFront - Used to get the recording preferences from the device.
 */
async function gInit(perfFront, preferenceFront) {
  const store = createStore(reducers);

  // Send the initial requests in parallel.
  const [recordingPreferences, supportedFeatures] = await Promise.all([
    // Pull the default recording settings from the background.jsm module. Update them
    // according to what's in the target's preferences. This way the preferences are
    // stored on the target. This could be useful for something like Android where you
    // might want to tweak the settings.
    getRecordingPreferencesFromDebuggee(
      preferenceFront,
      getDefaultRecordingPreferencesForOlderFirefox()
    ),
    // Get the supported features from the debuggee. If the debuggee is before
    // Firefox 72, then return null, as the actor does not support that feature.
    // We can't use `target.actorHasMethod`, because the target is not provided
    // when remote debugging. Unfortunately, this approach means that if this
    // function throws a real error, it will get swallowed here.
    Promise.resolve(perfFront.getSupportedFeatures()).catch(() => null),
  ]);

  // Do some initialization, especially with privileged things that are part of the
  // the browser.
  store.dispatch(
    actions.initializeStore({
      perfFront,
      receiveProfile,
      recordingPreferences,
      supportedFeatures,
      pageContext: "devtools",

      // Go ahead and hide the implementation details for the component on how the
      // preference information is stored
      /**
       * @param {RecordingStateFromPreferences} newRecordingPreferences
       */
      setRecordingPreferences: newRecordingPreferences =>
        setRecordingPreferencesOnDebuggee(
          preferenceFront,
          newRecordingPreferences
        ),

      // Configure the getSymbolTable function for the DevTools workflow.
      // See createMultiModalGetSymbolTableFn for more information.
      getSymbolTableGetter:
        /** @type {(profile: Object) => GetSymbolTableCallback} */
        profile =>
          createMultiModalGetSymbolTableFn(
            profile,
            () => selectors.getObjdirs(store.getState()),
            selectors.getPerfFront(store.getState())
          ),
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
}

function gDestroy() {
  ReactDOM.unmountComponentAtNode(document.querySelector("#root"));
}
