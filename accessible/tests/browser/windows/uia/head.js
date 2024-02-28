/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported gIsUiaEnabled, addUiaTask */

// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/accessible/tests/browser/shared-head.js",
  this
);

// Loading and common.js from accessible/tests/mochitest/ for all tests, as
// well as promisified-events.js.
loadScripts(
  { name: "common.js", dir: MOCHITESTS_DIR },
  { name: "promisified-events.js", dir: MOCHITESTS_DIR }
);

let gIsUiaEnabled = false;

/**
 * This is like addAccessibleTask, but takes two additional boolean options:
 * - uiaEnabled: Whether to run a variation of this test with Gecko UIA enabled.
 * - uiaDisabled: Whether to run a variation of this test with UIA disabled. In
 *   this case, UIA will rely entirely on the IA2 -> UIA proxy.
 * If both are set, the test will be run twice with different configurations.
 * You can determine which variant is currently running using the gIsUiaEnabled
 * variable. This is useful for conditional tests; e.g. if Gecko UIA supports
 * something that the IA2 -> UIA proxy doesn't support.
 */
function addUiaTask(doc, task, options = {}) {
  const { uiaEnabled = true, uiaDisabled = true } = options;

  function addTask(shouldEnable) {
    async function uiaTask(browser, docAcc, topDocAcc) {
      await SpecialPowers.pushPrefEnv({
        set: [["accessibility.uia.enable", shouldEnable]],
      });
      gIsUiaEnabled = shouldEnable;
      info(shouldEnable ? "Gecko UIA enabled" : "Gecko UIA disabled");
      await task(browser, docAcc, topDocAcc);
    }
    addAccessibleTask(doc, uiaTask, options);
  }

  if (uiaEnabled) {
    addTask(true);
  }
  if (uiaDisabled) {
    addTask(false);
  }
}
