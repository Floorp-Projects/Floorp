/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// shared-head.js handles imports, constants, and utility functions
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this
);

// So that PERFHERDER data can be extracted from the logs.
SimpleTest.requestCompleteLog();

function getFilteredModules(filters, loaders) {
  let modules = [];
  for (const l of loaders) {
    const loaderModulesMap = l.modules;
    const loaderModulesPaths = Object.keys(loaderModulesMap);
    modules = modules.concat(loaderModulesPaths);
  }
  return modules.filter(url => filters.some(filter => url.includes(filter)));
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

/**
 * Record module loading data.
 *
 * @param {Object}
 * - filterString {String} path to use to filter modules specific to the current panel
 * - loaders {Array} Array of Loaders to check for modules
 * - panelName {String} reused in identifiers for perfherder data
 */
function runMetricsTest({ filterString, loaders, panelName }) {
  const allModules = getFilteredModules([""], loaders);
  const panelModules = getFilteredModules([filterString], loaders);
  const vendoredModules = getFilteredModules(
    ["devtools/client/debugger/dist/vendors", "devtools/client/shared/vendor/"],
    loaders
  );

  const allModulesCount = allModules.length;
  const panelModulesCount = panelModules.length;
  const vendoredModulesCount = vendoredModules.length;

  const allModulesChars = countCharsInModules(allModules);
  const panelModulesChars = countCharsInModules(panelModules);
  const vendoredModulesChars = countCharsInModules(vendoredModules);

  const PERFHERDER_DATA = {
    framework: {
      name: "devtools",
    },
    suites: [
      {
        name: panelName + "-metrics",
        value: allModulesChars,
        subtests: [
          {
            name: panelName + "-modules",
            value: panelModulesCount,
          },
          {
            name: panelName + "-chars",
            value: panelModulesChars,
          },
          {
            name: "all-modules",
            value: allModulesCount,
          },
          {
            name: "all-chars",
            value: allModulesChars,
          },
          {
            name: "vendored-modules",
            value: vendoredModulesCount,
          },
          {
            name: "vendored-chars",
            value: vendoredModulesChars,
          },
        ],
      },
    ],
  };
  info("PERFHERDER_DATA: " + JSON.stringify(PERFHERDER_DATA));

  // Simply check that we found valid values.
  ok(
    allModulesCount > panelModulesCount && panelModulesCount > 0,
    "Successfully recorded module count for " + panelName
  );
  ok(
    allModulesChars > panelModulesChars && panelModulesChars > 0,
    "Successfully recorded char count for " + panelName
  );

  // Easy way to check how many vendored chars we have for a given panel.
  const percentage = ((100 * vendoredModulesChars) / allModulesChars).toFixed(
    1
  );
  info(`Percentage of vendored chars for ${panelName}: ${percentage}%`);
}

function getDuplicatedModules(loaders) {
  const allModules = getFilteredModules([""], loaders);

  const uniqueModules = new Set();
  const duplicatedModules = new Set();
  for (const mod of allModules) {
    if (uniqueModules.has(mod)) {
      duplicatedModules.add(mod);
    }

    uniqueModules.add(mod);
  }

  return duplicatedModules;
}

/**
 * Check that modules are only loaded once in a given set of loaders.
 * Panels might load the same module twice by mistake if they are both using
 * a BrowserLoader and the regular DevTools Loader.
 *
 * @param {Array} loaders
 *        Array of Loader instances.
 * @param {Array} allowedDupes
 *        Array of Strings which are paths to known duplicated modules.
 *        The test will also fail if a allowedDupesed module is not found in the
 *        duplicated modules.
 */
function runDuplicatedModulesTest(loaders, allowedDupes) {
  const duplicatedModules = getDuplicatedModules(loaders);

  // Remove allowedDupes entries, and fail if an allowed entry is not found.
  for (const mod of allowedDupes) {
    const deleted = duplicatedModules.delete(mod);
    if (!deleted) {
      ok(
        false,
        "module not found in the duplicated modules: [" +
          mod +
          "]. The allowedDupes array should be updated to remove it."
      );
    }
  }

  // Prepare a log string with the paths of all duplicated modules.
  let duplicatedModulesLog = "";
  for (const mod of duplicatedModules) {
    duplicatedModulesLog += `  [duplicated module] ${mod}\n`;
  }

  // Check that duplicatedModules Set is empty.
  is(
    duplicatedModules.size,
    0,
    "Duplicated module load detected. List of duplicated modules:\n" +
      duplicatedModulesLog
  );
}
