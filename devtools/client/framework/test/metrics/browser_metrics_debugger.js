/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test records the number of modules loaded by DevTools, as well as the total count
 * of characters in those modules, when opening the debugger. These metrics are
 * retrieved by perfherder via logs.
 */

const TEST_URL = "data:text/html;charset=UTF-8,<div>Debugger modules load test</div>";

add_task(async function() {
  const toolbox = await openNewTabAndToolbox(TEST_URL, "jsdebugger");
  const panel = toolbox.getCurrentPanel();

  // Retrieve the browser loader dedicated to the Debugger.
  const debuggerLoader = panel.panelWin.getBrowserLoaderForWindow();
  const loaders = [loader.provider.loader, debuggerLoader.loader];

  runMetricsTest({
    filterString: "devtools/client/debugger",
    loaders,
    panelName: "debugger",
  });
});
