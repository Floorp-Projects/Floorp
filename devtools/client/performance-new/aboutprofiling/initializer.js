/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check
/**
 * @typedef {import("../@types/perf").RecordingSettings} RecordingSettings
 * @typedef {import("../@types/perf").PerfFront} PerfFront
 * @typedef {import("../@types/perf").PageContext} PageContext
 */
"use strict";

/**
 * This file initializes the about:profiling page, which can be used to tweak the
 * profiler's settings.
 */

{
  // Create the browser loader, but take care not to conflict with
  // TypeScript. See devtools/client/performance-new/typescript.md and
  // the section on "Do not overload require" for more information.

  const { BrowserLoader } = ChromeUtils.import(
    "resource://devtools/shared/loader/browser-loader.js"
  );
  const browserLoader = BrowserLoader({
    baseURI: "resource://devtools/client/performance-new/aboutprofiling",
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
 * for various components. This page needs a copy, and it is also used by the
 * profiler shortcuts. In order to do this, the background code needs to live in a
 * JSM module, that can be shared with the DevTools keyboard shortcut manager.
 */
const { presets } = ChromeUtils.import(
  "resource://devtools/client/performance-new/popup/background.jsm.js"
);

const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const React = require("devtools/client/shared/vendor/react");
const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const {
  FluentL10n,
} = require("devtools/client/shared/fluent-l10n/fluent-l10n");
const Provider = React.createFactory(
  require("devtools/client/shared/vendor/react-redux").Provider
);
const ProfilerPreferenceObserver = React.createFactory(
  require("devtools/client/performance-new/components/ProfilerPreferenceObserver")
);
const LocalizationProvider = React.createFactory(
  FluentReact.LocalizationProvider
);
const AboutProfiling = React.createFactory(
  require("devtools/client/performance-new/components/AboutProfiling")
);
const createStore = require("devtools/client/shared/redux/create-store");
const reducers = require("devtools/client/performance-new/store/reducers");
const actions = require("devtools/client/performance-new/store/actions");
const { Ci } = require("chrome");

/**
 * Initialize the panel by creating a redux store, and render the root component.
 *
 * @param {PageContext} pageContext - The context that the UI is being loaded in under.
 * @param {boolean} isSupportedPlatform
 * @param {string[]} supportedFeatures
 * @param {(() => void)} [openRemoteDevTools] Optionally provide a way to go back to
 *                                            the remote devtools page.
 */
async function gInit(
  pageContext,
  isSupportedPlatform,
  supportedFeatures,
  openRemoteDevTools
) {
  const store = createStore(reducers);

  const l10n = new FluentL10n();
  await l10n.init(
    [
      "devtools/client/perftools.ftl",
      // For -brand-shorter-name used in some profiler preset descriptions.
      "branding/brand.ftl",
      // Needed for the onboarding UI
      "browser/branding/brandings.ftl",
    ],
    {
      setAttributesOnDocument: true,
    }
  );

  // Do some initialization, especially with privileged things that are part of the
  // the browser.
  store.dispatch(
    actions.initializeStore({
      isSupportedPlatform,
      supportedFeatures,
      presets,
      pageContext,
      openRemoteDevTools,
    })
  );

  ReactDOM.render(
    Provider(
      { store },
      LocalizationProvider(
        { bundles: l10n.getBundles() },
        React.createElement(
          React.Fragment,
          null,
          ProfilerPreferenceObserver(),
          AboutProfiling()
        )
      )
    ),
    document.querySelector("#root")
  );

  window.addEventListener("unload", () => gDestroy(), { once: true });
}

async function gDestroy() {
  // This allows all unregister commands to run.
  ReactDOM.unmountComponentAtNode(document.querySelector("#root"));
}

// Automatically initialize the page if it's not a remote connection, otherwise
// the page will be initialized by about:debugging.
if (window.location.hash !== "#remote") {
  document.addEventListener(
    "DOMContentLoaded",
    () => {
      const isSupportedPlatform = "nsIProfiler" in Ci;
      const supportedFeatures = isSupportedPlatform
        ? Services.profiler.GetFeatures()
        : [];
      gInit("aboutprofiling", isSupportedPlatform, supportedFeatures);
    },
    { once: true }
  );
}
