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
  await addBreakpoint(dbg, "simple1.js", 4);

  info("Call the function that we set a breakpoint in.");
  invokeInTab("main");
  await waitForPaused(dbg);
  await waitForSelectedSource(dbg, "simple1.js");
  assertPausedAtSourceAndLine(dbg, findSource(dbg, "simple1.js").id, 4);

  info("Step into another file.");
  await stepOver(dbg);
  await stepIn(dbg);
  await waitForSelectedSource(dbg, "simple2.js");
  assertPausedAtSourceAndLine(dbg, findSource(dbg, "simple2.js").id, 3);

  info("Step out to the initial file.");
  await stepOut(dbg);
  assertPausedAtSourceAndLine(dbg, findSource(dbg, "simple1.js").id, 6);
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
