/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../../../shared/test/shared-head.js */
/* import-globals-from ../../../shared/test/telemetry-test-helpers.js */

// shared-head.js handles imports, constants, and utility functions
Services.scriptloader.loadSubScript("chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js", this);

// So that PERFHERDER data can be extracted from the logs.
SimpleTest.requestCompleteLog();

function getFilteredModules(filter, loaders) {
  let modules = [];
  for (const l of loaders) {
    const loaderModulesMap = l.modules;
    const loaderModulesPaths = Object.keys(loaderModulesMap);
    modules = modules.concat(loaderModulesPaths);
  }
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
