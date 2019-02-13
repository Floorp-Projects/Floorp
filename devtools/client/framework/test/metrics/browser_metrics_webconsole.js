/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../../shared/test/shared-head.js */

/**
 * This test records the number of modules loaded by DevTools, as well as the total count
 * of characters in those modules, when opening the webconsole. These metrics are
 * retrieved by perfherder via logs.
 */

const TEST_URL = "data:text/html;charset=UTF-8,<div>Webconsole modules load test</div>";

add_task(async function() {
  const toolbox = await openNewTabAndToolbox(TEST_URL, "webconsole");
  const hud = toolbox.getCurrentPanel().hud;

  // Retrieve the browser loader dedicated to the WebConsole.
  const webconsoleLoader = hud.ui.browserLoader;
  const loaders = [loader.provider.loader, webconsoleLoader.loader];

  const allModules = getFilteredModules("", loaders);
  const webconsoleModules = getFilteredModules("devtools/client/webconsole", loaders);

  const allModulesCount = allModules.length;
  const webconsoleModulesCount = webconsoleModules.length;

  const allModulesChars = countCharsInModules(allModules);
  const webconsoleModulesChars = countCharsInModules(webconsoleModules);

  const PERFHERDER_DATA = {
    framework: {
      name: "devtools",
    },
    suites: [{
      name: "webconsole-metrics",
      value: allModulesChars,
      subtests: [
        {
          name: "webconsole-modules",
          value: webconsoleModulesCount,
        },
        {
          name: "webconsole-chars",
          value: webconsoleModulesChars,
        },
        {
          name: "all-modules",
          value: allModulesCount,
        },
        {
          name: "all-chars",
          value: allModulesChars,
        },
      ],
    }],
  };
  info("PERFHERDER_DATA: " + JSON.stringify(PERFHERDER_DATA));

  // Simply check that we found valid values.
  ok(allModulesCount > webconsoleModulesCount &&
     webconsoleModulesCount > 0, "Successfully recorded module count for WebConsole");
  ok(allModulesChars > webconsoleModulesChars &&
     webconsoleModulesChars > 0, "Successfully recorded char count for WebConsole");
});
