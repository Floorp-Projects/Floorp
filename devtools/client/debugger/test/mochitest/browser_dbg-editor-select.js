/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that the editor highlights the correct location when the
// debugger pauses

"use strict";

requestLongerTimeout(2);

add_task(async function () {
  // This test runs too slowly on linux debug. I'd like to figure out
  // which is the slowest part of this and make it run faster, but to
  // fix a frequent failure allow a longer timeout.
  const dbg = await initDebugger("doc-scripts.html");

  info("Set the initial breakpoint.");
  await selectSource(dbg, "simple1.js");
  ok(
    !findElementWithSelector(dbg, ".cursor-position"),
    "no position is displayed when selecting a location without an explicit line number"
  );
  await selectSource(dbg, "simple1.js", 1, 1);
  assertCursorPosition(
    dbg,
    1,
    2,
    "when passing an explicit line number, the position is displayed"
  );
  // Note that CodeMirror is 0-based while the footer displays 1-based
  getCM(dbg).setCursor({ line: 1, ch: 0 });
  assertCursorPosition(
    dbg,
    2,
    1,
    "when moving the cursor, the position footer updates"
  );
  getCM(dbg).setCursor({ line: 2, ch: 0 });
  assertCursorPosition(
    dbg,
    3,
    1,
    "when moving the cursor a second time, the position footer still updates"
  );

  await addBreakpoint(dbg, "simple1.js", 4);
  const breakpointItems = findAllElements(dbg, "breakpointItems");
  breakpointItems[0].click();
  await waitForCursorPosition(dbg, 4);
  assertCursorPosition(
    dbg,
    4,
    16,
    "when moving the cursor a second time, the position footer still updates"
  );

  info("Call the function that we set a breakpoint in.");
  invokeInTab("main");
  await waitForPaused(dbg);
  await waitForSelectedSource(dbg, "simple1.js");
  assertPausedAtSourceAndLine(dbg, findSource(dbg, "simple1.js").id, 4);
  assertCursorPosition(dbg, 4, 16, "on pause, the cursor updates");

  info("Step into another file.");
  await stepOver(dbg);
  await stepIn(dbg);
  await waitForSelectedSource(dbg, "simple2.js");
  assertPausedAtSourceAndLine(dbg, findSource(dbg, "simple2.js").id, 3);
  assertCursorPosition(dbg, 3, 5, "on step-in, the cursor updates");

  info("Step out to the initial file.");
  await stepOut(dbg);
  assertPausedAtSourceAndLine(dbg, findSource(dbg, "simple1.js").id, 6);
  assertCursorPosition(dbg, 6, 3, "on step-out, the cursor updates");
  await resume(dbg);

  info("Make sure that the editor scrolls to the paused location.");
  const longSrc = findSource(dbg, "long.js");
  await selectSource(dbg, "long.js");
  await addBreakpoint(dbg, longSrc, 66);

  invokeInTab("testModel");
  await waitForPaused(dbg);
  await waitForSelectedSource(dbg, "long.js");

  assertPausedAtSourceAndLine(dbg, findSource(dbg, "long.js").id, 66);
  ok(
    isVisibleInEditor(dbg, findElement(dbg, "breakpoint")),
    "Breakpoint is visible"
  );
});
