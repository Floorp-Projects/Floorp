/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests to make sure we do not accidentally slide the breakpoint up to the first
// function with the same name in the file.
// TODO: Likely to remove this test when removing the breakpoint sliding functionality

"use strict";

add_task(async function () {
  const dbg = await initDebugger(
    "doc-duplicate-functions.html",
    "doc-duplicate-functions.html"
  );
  let source = findSource(dbg, "doc-duplicate-functions.html");

  await selectSource(dbg, source);
  await addBreakpoint(dbg, source, 21);

  await reload(dbg, "doc-duplicate-functions.html");

  await waitForState(dbg, state => dbg.selectors.getBreakpointCount() == 1);

  const firstBreakpoint = dbg.selectors.getBreakpointsList()[0];
  is(firstBreakpoint.location.line, 21, "Breakpoint is on line 21");

  // Make sure the breakpoint set on line 19 gets hit
  await invokeInTab("b");
  invokeInTab("func");
  await waitForPaused(dbg);

  source = findSource(dbg, "doc-duplicate-functions.html");
  assertPausedAtSourceAndLine(dbg, source.id, 21);
  await assertBreakpoint(dbg, 21);

  await resume(dbg);
});
