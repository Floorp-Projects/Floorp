/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../../shared/test/shared-head.js */

/**
 * This test records the number of modules loaded by DevTools, as well as the total count
 * of characters in those modules, when opening the inspector. These metrics are retrieved
 * by perfherder via logs.
 */

const TEST_URL = "data:text/html;charset=UTF-8,<div>Inspector modules load test</div>";

add_task(async function() {
  await openNewTabAndToolbox(TEST_URL, "inspector");

  const allModules = getFilteredModules("");
  const inspectorModules = getFilteredModules("devtools/client/inspector");

  const allModulesCount = allModules.length;
  const inspectorModulesCount = inspectorModules.length;

  const allModulesChars = countCharsInModules(allModules);
  const inspectorModulesChars = countCharsInModules(inspectorModules);

  const PERFHERDER_DATA = {
    framework: {
      name: "devtools"
    },
    suites: [{
      name: "inspector-metrics",
      value: allModulesChars,
      subtests: [
        {
          name: "inspector-modules",
          value: inspectorModulesCount
        },
        {
          name: "inspector-chars",
          value: inspectorModulesChars
        },
        {
          name: "all-modules",
          value: allModulesCount
        },
        {
          name: "all-chars",
          value: allModulesChars
        },
      ],
    }]
  };
  info("PERFHERDER_DATA: " + JSON.stringify(PERFHERDER_DATA));

  // Simply check that we found valid values.
  ok(allModulesCount > inspectorModulesCount &&
     inspectorModulesCount > 0, "Successfully recorded module count for Inspector");
  ok(allModulesChars > inspectorModulesChars &&
     inspectorModulesChars > 0, "Successfully recorded char count for Inspector");
});

function getFilteredModules(filter) {
  const modules = Object.keys(loader.provider.loader.modules);
  return modules.filter(url => url.includes(filter));
}

function countCharsInModules(modules) {
  return modules.reduce((sum, uri) => {
    try {
      return sum + require("raw!" + uri).length;
    } catch (e) {
      // Ignore failures
      return sum;
    }
  }, 0);
}
