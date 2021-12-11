/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Check evaluating eager-evaluation values.
const TEST_URI = "data:text/html;charset=utf8,";

add_task(async function() {
  await addTab(TEST_URI);

  await pushPref("devtools.chrome.enabled", true);

  info("Open the Browser Console");
  const hud = await BrowserConsoleManager.toggleBrowserConsole();

  await executeNonDebuggeeSideeffect(hud);
});

// Test that code is still terminated, even if it is calling into realms
// that aren't the normal debuggee realms (bug 1620087).
async function executeNonDebuggeeSideeffect(hud) {
  await executeAndWaitForMessage(
    hud,
    `globalThis.eagerLoader = ChromeUtils.import("resource://devtools/shared/Loader.jsm");`,
    `DevToolsLoader`
  );

  // "require" should terminate execution because it will try to create a new
  // module record for the given URL, as long as the loader's debuggee
  // has been properly added to the debugger. The termination should
  // happen before it starts processing the path, so we don't need to provide
  // a real path here.
  setInputValue(hud, `globalThis.eagerLoader.require("fake://path");`);

  // Wait a bit to make sure that the command has time to fail before we
  // validate the eager-eval result.
  await wait(500);
  await waitForEagerEvaluationResult(hud, "");

  setInputValue(hud, "");

  await executeAndWaitForMessage(hud, `delete globalThis.eagerLoader;`, `true`);
}
