/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Check evaluating eager-evaluation values.
const TEST_URI = "data:text/html;charset=utf8,<!DOCTYPE html>";

add_task(async function () {
  await addTab(TEST_URI);

  await pushPref("devtools.chrome.enabled", true);

  info("Open the Browser Console");
  const hud = await BrowserConsoleManager.toggleBrowserConsole();

  await executeResolveHookWithSideEffect(hud);
});

async function executeResolveHookWithSideEffect(hud) {
  // Services.droppedLinkHandler is implemented with resolve hook, which imports
  // ContentAreaDropListener.sys.mjs.
  //
  // In order to test the resolve hook behavior, ensure the module is not yet
  // loaded, which ensures the property is not yet resolved.
  //
  // NOTE: This test is not compatible with verify mode, given it depends on the
  //       initial state of the Services object and the module.
  is(
    Cu.isESModuleLoaded(
      "resource://gre/modules/ContentAreaDropListener.sys.mjs"
    ),
    false
  );

  setInputValue(hud, `Services.droppedLinkHandler`);

  await wait(500);
  // Eager evaluation should fail, due to the side effect in the resolve hook.
  await waitForEagerEvaluationResult(hud, "");

  setInputValue(hud, "");
  await wait(500);

  // The property should be resolved when evaluating after the eager evaluation.
  await executeAndWaitForResultMessage(
    hud,
    `Services.droppedLinkHandler;`,
    "XPCWrappedNative_NoHelper"
  );

  is(
    Cu.isESModuleLoaded(
      "resource://gre/modules/ContentAreaDropListener.sys.mjs"
    ),
    true
  );

  // Eager evaluation should work after the property is resolved.
  setInputValue(hud, `Services.droppedLinkHandler`);
  await wait(500);
  await waitForEagerEvaluationResult(hud, /XPCWrappedNative_NoHelper/);
}
