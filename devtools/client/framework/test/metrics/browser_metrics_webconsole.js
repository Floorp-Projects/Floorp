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
  const panel = toolbox.getCurrentPanel();

  // Retrieve the browser loader dedicated to the WebConsole.
  const webconsoleLoader = panel._frameWindow.getBrowserLoaderForWindow();
  const loaders = [loader.loader, webconsoleLoader.loader];

  runMetricsTest({
    filterString: "devtools/client/webconsole",
    loaders,
    panelName: "webconsole",
  });
});
