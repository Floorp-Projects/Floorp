/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Tests for preview through Babel's compile output.
requestLongerTimeout(5);

// Test pausing with mapScopes enabled and disabled
add_task(async function () {
  const dbg = await initDebugger("doc-sourcemapped.html");
  dbg.actions.toggleMapScopes();

  info("1. Pause on line 20");
  const url = "webpack3-babel6://./esmodules-cjs/input.js";
  await waitForSources(dbg, url);
  const source = findSource(dbg, url);
  await selectSource(dbg, source);
  await addBreakpoint(dbg, source, 20, 2);
  invokeInTab("webpack3Babel6EsmodulesCjs");
  await waitForPaused(dbg);

  info("2. Hover on a token with mapScopes enabled");
  await assertPreviewTextValue(dbg, 20, 16, {
    text: '"a-default"',
    expression: "aDefault",
  });
  ok(getOriginalScope(dbg) != null, "Scopes are mapped");

  info("3. Hover on a token with mapScopes disabled");
  clickElement(dbg, "mapScopesCheckbox");
  await assertPreviewTextValue(dbg, 21, 16, {
    text: "undefined",
    expression: "anAliased",
  });

  info("4. StepOver with mapScopes disabled");
  await stepOver(dbg);
  await assertPreviewTextValue(dbg, 20, 16, {
    text: "undefined",
    expression: "aDefault",
  });
  ok(getOriginalScope(dbg) == null, "Scopes are not mapped");
});

function getOriginalScope(dbg) {
  return dbg.selectors.getSelectedOriginalScope(
    dbg.selectors.getCurrentThread()
  );
}
