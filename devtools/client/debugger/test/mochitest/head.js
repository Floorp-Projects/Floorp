/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* eslint-disable no-unused-vars */

"use strict";

// This head.js file is only imported by debugger mochitests.
// Anything that is meant to be used by tests of other panels should be moved to shared-head.js
// Also, any symbol that may conflict with other test symbols should stay in head.js
// (like EXAMPLE_URL)

const EXAMPLE_URL =
  "https://example.com/browser/devtools/client/debugger/test/mochitest/examples/";

// This URL is remote compared to EXAMPLE_URL, as one uses .com and the other uses .org
// Note that this depends on initDebugger to always use EXAMPLE_URL
const EXAMPLE_REMOTE_URL =
  "https://example.org/browser/devtools/client/debugger/test/mochitest/examples/";

// shared-head.js handles imports, constants, and utility functions
/* import-globals-from ../../../shared/test/shared-head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this
);

/* import-globals-from ./shared-head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/debugger/test/mochitest/shared-head.js",
  this
);

/* import-globals-from ../../../webconsole/test/browser/shared-head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/webconsole/test/browser/shared-head.js",
  this
);

/**
 * Helper function for `_loadAllIntegrationTests`.
 *
 * Implements this as a global method in order to please eslint.
 * This will be used by modules loaded from "integration-tests" folder
 * in order to register a new integration task, ran when executing `runAllIntegrationTests`.
 */
const integrationTasks = [];
function addIntegrationTask(fun) {
  integrationTasks.push(fun);
}

/**
 * Helper function for `runAllIntegrationTests`.
 *
 * Loads all the modules from "integration-tests" folder and return
 * all the task they implemented and registered by calling `addIntegrationTask`.
 */
function _loadAllIntegrationTests() {
  const testsDir = getChromeDir(getResolvedURI(gTestPath));
  testsDir.append("integration-tests");
  const entries = testsDir.directoryEntries;
  const urls = [];
  while (entries.hasMoreElements()) {
    const file = entries.nextFile;
    const url = Services.io.newFileURI(file).spec;
    if (url.endsWith(".js")) {
      urls.push(url);
    }
  }

  // We need to sort in order to run the test in a reliable order
  urls.sort();

  for (const url of urls) {
    Services.scriptloader.loadSubScript(url, this);
  }
  return integrationTasks;
}

/**
 * Method to be called by each integration tests which will
 * run all the "integration tasks" implemented in files from the "integration-tests" folder.
 * These files should call the `addIntegrationTask()` method to register something to run.
 *
 * @param {String} testFolder
 *        Define what folder in "examples" folder to load before opening the debugger.
 *        This is meant to be a versionized test folder with v1, v2, v3 folders.
 *        (See createVersionizedHttpTestServer())
 * @param {Object} env
 *        Environment object passed down to each task to better know
 *        which particular integration test is being run.
 */
async function runAllIntegrationTests(testFolder, env) {
  const tasks = _loadAllIntegrationTests();

  const testServer = createVersionizedHttpTestServer("examples/" + testFolder);
  const testUrl = testServer.urlFor("index.html");

  for (const task of tasks) {
    info(` ==> Running integration task '${task.name}'`);
    await task(testServer, testUrl, env);
  }
}

const INTEGRATION_TEST_PAGE_SOURCES = [
  "index.html",
  "iframe.html",
  "script.js",
  "onload.js",
  "test-functions.js",
  "query.js?x=1",
  "query.js?x=2",
  "query2.js?y=3",
  "bundle.js",
  "original.js",
  "bundle-with-another-original.js",
  "original-with-no-update.js",
  "replaced-bundle.js",
  "removed-original.js",
  "named-eval.js",
  // This is the JS file with encoded characters and custom protocol
  "文字コード.js",
  // Webpack generated some extra sources:
  "bootstrap 3b1a221408fdde86aa49",
  "bootstrap a1ecee2f86e1d0ea3fb5",
  "bootstrap 6fda1f7ea9ecbc1a2d5b",
  // There is 3 occurences, one per target (main thread, worker and iframe).
  // But there is even more source actors (named evals and duplicated script tags).
  "same-url.sjs",
  "same-url.sjs",
];
// The iframe one is only available when fission is enabled, or EFT
if (isFissionEnabled() || isEveryFrameTargetEnabled()) {
  INTEGRATION_TEST_PAGE_SOURCES.push("same-url.sjs");
}

/**
 * Install a Web Extension which will run a content script against any test page
 * served from https://example.com
 *
 * This content script is meant to be debuggable when devtools.chrome.enabled is true.
 */
async function installAndStartContentScriptExtension() {
  function contentScript() {
    console.log("content script loads");

    // This listener prevents the source from being garbage collected
    // and be missing from the scripts returned by `dbg.findScripts()`
    // in `ThreadActor._discoverSources`.
    window.onload = () => {};
  }

  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test content script extension",
      content_scripts: [
        {
          js: ["content_script.js"],
          matches: ["https://example.com/*"],
          run_at: "document_start",
        },
      ],
    },
    files: {
      "content_script.js": contentScript,
    },
  });

  await extension.startup();

  return extension;
}

/**
 * Return the text content for a given line in the Source Tree.
 *
 * @param {Object} dbg
 * @param {Number} index
 *        Line number in the source tree
 */
function getSourceTreeLabel(dbg, index) {
  return (
    findElement(dbg, "sourceNode", index)
      .textContent.trim()
      // There is some special whitespace character which aren't removed by trim()
      .replace(/^[\s\u200b]*/g, "")
  );
}

/**
 * Find and assert the source tree node with the specified text
 * exists on the source tree.
 *
 * @param {Object} dbg
 * @param {String} text The node text displayed
 */
async function assertSourceTreeNode(dbg, text) {
  let node = null;
  await waitUntil(() => {
    node = findSourceNodeWithText(dbg, text);
    return !!node;
  });
  ok(!!node, `Source tree node with text "${text}" exists`);
}
