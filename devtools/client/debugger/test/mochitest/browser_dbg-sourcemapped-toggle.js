/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Tests for preview through Babel's compile output.
requestLongerTimeout(5);

// Test pausing with mapScopes enabled and disabled
add_task(async function () {
  const dbg = await initDebugger("doc-sourcemapped.html");

  info("1. Pause on line 20");
  const url = "webpack3-babel6://./esmodules-cjs/input.js";
  await waitForSources(dbg, url);
  const source = findSource(dbg, url);
  await selectSource(dbg, source);
  await addBreakpoint(dbg, source, 20, 3);
  invokeInTab("webpack3Babel6EsmodulesCjs");
  await waitForPausedInOriginalFileAndToggleMapScopes(dbg);

  ok(getOriginalScope(dbg) != null, "Scopes are now mapped");

  await assertPreviewTextValue(dbg, 20, 17, {
    result: '"a-default"',
    expression: "aDefault",
  });

  info("3. Hover on a token with mapScopes disabled");
  await toggleMapScopes(dbg);
  // Add assertion for inline preview disabled and footer notification
  info("4. StepOver with mapScopes disabled");
  await stepOver(dbg, { shouldWaitForLoadedScopes: false });
  // Add assertion for inline preview disabled and footer notification
});

function getOriginalScope(dbg) {
  return dbg.selectors.getSelectedOriginalScope(
    dbg.selectors.getCurrentThread()
  );
}
