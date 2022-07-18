/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test records the number of modules loaded by DevTools, as well as the total count
 * of characters in those modules, when opening the webconsole. These metrics are
 * retrieved by perfherder via logs.
 */

const TEST_URL =
  "data:text/html;charset=UTF-8,<div>Webconsole modules load test</div>";

add_task(async function() {
  const toolbox = await openNewTabAndToolbox(TEST_URL, "webconsole");
  const toolboxBrowserLoader = toolbox.win.getBrowserLoaderForWindow();

  // Retrieve the browser loader dedicated to the WebConsole.
  const panel = toolbox.getCurrentPanel();
  const webconsoleLoader = panel._frameWindow.getBrowserLoaderForWindow();

  const loaders = [
    loader.loader,
    toolboxBrowserLoader.loader,
    webconsoleLoader.loader,
  ];

  const allowedDupes = [
    "@loader/unload.js",
    "@loader/options.js",
    "chrome.js",
    "resource://devtools/client/webconsole/constants.js",
    "resource://devtools/client/webconsole/utils.js",
    "resource://devtools/client/webconsole/utils/messages.js",
    "resource://devtools/client/webconsole/utils/l10n.js",
    "resource://devtools/client/netmonitor/src/utils/request-utils.js",
    "resource://devtools/client/webconsole/types.js",
    "resource://devtools/client/shared/components/menu/MenuButton.js",
    "resource://devtools/client/shared/components/menu/MenuItem.js",
    "resource://devtools/client/shared/components/menu/MenuList.js",
    "resource://devtools/client/shared/vendor/fluent-react.js",
    "resource://devtools/client/shared/vendor/react.js",
    "resource://devtools/client/shared/vendor/react-dom.js",
    "resource://devtools/client/shared/vendor/react-prop-types.js",
    "resource://devtools/client/shared/vendor/react-dom-factories.js",
    "resource://devtools/client/shared/vendor/redux.js",
    "resource://devtools/client/shared/redux/middleware/thunk.js",
  ];
  runDuplicatedModulesTest(loaders, allowedDupes);

  runMetricsTest({
    filterString: "devtools/client/webconsole",
    loaders,
    panelName: "webconsole",
  });
});
