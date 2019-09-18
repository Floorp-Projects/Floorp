/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test records the number of modules loaded by DevTools, as well as the total count
 * of characters in those modules, when opening the debugger. These metrics are
 * retrieved by perfherder via logs.
 */

const TEST_URL =
  "data:text/html;charset=UTF-8,<div>Debugger modules load test</div>";

add_task(async function() {
  const toolbox = await openNewTabAndToolbox(TEST_URL, "jsdebugger");
  const panel = toolbox.getCurrentPanel();

  // Retrieve the browser loader dedicated to the Debugger.
  const debuggerLoader = panel.panelWin.getBrowserLoaderForWindow();
  const loaders = [loader.loader, debuggerLoader.loader];

  const whitelist = [
    "@loader/unload.js",
    "@loader/options.js",
    "chrome.js",
    "resource://devtools/client/shared/vendor/react-dom.js",
    "resource://devtools/client/shared/vendor/react.js",
    "resource://devtools/client/shared/vendor/lodash.js",
    "resource://devtools/client/debugger/dist/vendors.js",
    "resource://devtools/client/shared/vendor/react-prop-types.js",
    "resource://devtools/client/shared/vendor/react-dom-factories.js",
    "resource://devtools/client/shared/vendor/react-redux.js",
    "resource://devtools/client/shared/vendor/redux.js",
    "resource://devtools/client/debugger/src/workers/parser/index.js",
  ];
  runDuplicatedModulesTest(loaders, whitelist);

  runMetricsTest({
    filterString: "devtools/client/debugger",
    loaders,
    panelName: "debugger",
  });
});
