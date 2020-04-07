/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test records the number of modules loaded by DevTools, as well as the total count
 * of characters in those modules, when opening the inspector. These metrics are retrieved
 * by perfherder via logs.
 */

const TEST_URL =
  "data:text/html;charset=UTF-8,<div>Inspector modules load test</div>";

add_task(async function() {
  const toolbox = await openNewTabAndToolbox(TEST_URL, "inspector");
  const toolboxBrowserLoader = toolbox.win.getBrowserLoaderForWindow();

  // Most panels involve three loaders:
  // - the global devtools loader
  // - the browser loader used by the toolbox
  // - a specific browser loader created for the panel
  // But the inspector is a specific case, because it reuses the BrowserLoader
  // of the toolbox to load its react components. This is why we only list
  // two loaders here.
  const loaders = [loader.loader, toolboxBrowserLoader.loader];

  runDuplicatedModulesTest(loaders, [
    "@loader/unload.js",
    "@loader/options.js",
    "chrome.js",
    "resource://devtools/client/shared/vendor/react.js",
    "resource://devtools/client/shared/vendor/react-dom-factories.js",
    "resource://devtools/client/shared/vendor/react-prop-types.js",
    "resource://devtools/client/shared/vendor/redux.js",
  ]);

  runMetricsTest({
    filterString: "devtools/client/inspector",
    loaders,
    panelName: "inspector",
  });
});
