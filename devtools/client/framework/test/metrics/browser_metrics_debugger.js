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
  // Disable randomly spawning processes during tests
  await pushPref("dom.ipc.processPrelaunch.enabled", false);

  const toolbox = await openNewTabAndToolbox(TEST_URL, "jsdebugger");
  const toolboxBrowserLoader = toolbox.win.getBrowserLoaderForWindow();

  // Retrieve the browser loader dedicated to the Debugger.
  const panel = toolbox.getCurrentPanel();
  const debuggerLoader = panel.panelWin.getBrowserLoaderForWindow();

  const loaders = [
    loader.loader,
    toolboxBrowserLoader.loader,
    debuggerLoader.loader,
  ];

  const allowedDupes = [
    "@loader/unload.js",
    "@loader/options.js",
    "chrome.js",
    "resource://devtools/client/shared/vendor/fluent-react.js",
    "resource://devtools/client/shared/vendor/react-dom.js",
    "resource://devtools/client/shared/vendor/react.js",
    "resource://devtools/client/shared/vendor/react-prop-types.js",
    "resource://devtools/client/shared/vendor/react-dom-factories.js",
    "resource://devtools/client/shared/vendor/react-redux.js",
    "resource://devtools/client/shared/vendor/redux.js",
    "resource://devtools/client/shared/redux/subscriber.js",
    "resource://devtools/client/shared/worker-utils.js",
    "resource://devtools/client/debugger/src/workers/parser/index.js",
    "resource://devtools/client/shared/source-map/index.js",
    "resource://devtools/client/shared/components/menu/MenuButton.js",
    "resource://devtools/client/shared/components/menu/MenuItem.js",
    "resource://devtools/client/shared/components/menu/MenuList.js",
  ];
  runDuplicatedModulesTest(loaders, allowedDupes);

  runMetricsTest({
    filterString: "devtools/client/debugger",
    loaders,
    panelName: "debugger",
  });

  // See Bug 1637793 and Bug 1621337.
  // Ideally the debugger should only resolve when the worker targets have been
  // retrieved, which should be fixed by Bug 1621337 or a followup.
  info("Wait for all pending requests to settle on the DevToolsClient");
  await toolbox.commands.client.waitForRequestsToSettle();
});
