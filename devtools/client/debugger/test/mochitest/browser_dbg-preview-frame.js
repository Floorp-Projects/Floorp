/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test hovering in a selected frame

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-script-switching.html");

  const found = findElement(dbg, "callStackBody");
  is(found, null, "Call stack is hidden");

  invokeInTab("firstCall");
  await waitForPaused(dbg);

  info("Preview a variable in the second frame");
  clickElement(dbg, "frame", 2);
  await waitForSelectedFrame(dbg, "firstCall");
  await assertPreviewTooltip(dbg, 8, 4, {
    result: "secondCall()",
    expression: "secondCall",
  });

  info("Preview should still work after selecting different locations");
  const frame = dbg.selectors.getVisibleSelectedFrame();
  const inScopeLines = dbg.selectors.getInScopeLines(frame.location);
  await selectSource(dbg, "script-switching-01.js");
  await assertPreviewTooltip(dbg, 8, 4, {
    result: "secondCall()",
    expression: "secondCall",
  });
  is(
    dbg.selectors.getInScopeLines(frame.location),
    inScopeLines,
    "In scope lines"
  );
});

function waitForSelectedFrame(dbg, displayName) {
  const { getInScopeLines, getVisibleSelectedFrame } = dbg.selectors;
  return waitForState(dbg, state => {
    const frame = getVisibleSelectedFrame();

    return frame?.displayName == displayName && getInScopeLines(frame.location);
  });
}
