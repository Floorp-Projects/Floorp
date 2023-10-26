/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that breakpoints set in the pretty printed files display and paused
// correctly.

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-pretty.html", "pretty.js");

  await selectSource(dbg, "pretty.js");
  await prettyPrint(dbg);

  info(
    "Add breakpoint in funcWithMultipleBreakableColumns, on the for-loop line"
  );
  const LINE_INDEX_TO_BREAK_ON = 15;
  const prettySourceFileName = "pretty.js:formatted";
  await addBreakpoint(dbg, prettySourceFileName, LINE_INDEX_TO_BREAK_ON);
  const prettySource = findSource(dbg, prettySourceFileName);

  info("Check that multiple column breakpoints can be set");
  const columnBreakpointMarkers = await waitForAllElements(
    dbg,
    "columnBreakpoints"
  );
  /*
   * We're pausing on the following line, which should have those breakpoints (marked with ➤)
   *
   * for( ➤let i=0; ➤i < items.length; ➤i++ ) {
   *
   */
  is(
    columnBreakpointMarkers.length,
    3,
    "We have the expected numbers of possible column breakpoints"
  );

  info("Enable the second column breakpoint");
  columnBreakpointMarkers[1].click();
  await waitForBreakpointCount(dbg, 2);
  await waitForAllElements(dbg, "breakpointItems", 2);

  info("Check that we do pause at expected locations");
  invokeInTab("funcWithMultipleBreakableColumns");

  info("We pause on the first column breakpoint (before `i` init)");
  await waitForPaused(dbg);
  await waitForInlinePreviews(dbg);
  await assertPausedAtSourceAndLine(
    dbg,
    prettySource.id,
    LINE_INDEX_TO_BREAK_ON,
    16
  );
  await resume(dbg);

  info(
    "We pause at the second column breakpoint, before the first loop iteration"
  );
  await waitForPaused(dbg);
  await waitForInlinePreviews(dbg);
  await assertPausedAtSourceAndLine(
    dbg,
    prettySource.id,
    LINE_INDEX_TO_BREAK_ON,
    19
  );
  const assertScopesForSecondColumnBreakpoint = topBlockItems =>
    assertScopes(dbg, [
      "Block",
      ["<this>", "Window"],
      ...topBlockItems,
      "funcWithMultipleBreakableColumns",
      ["arguments", "Arguments"],
      ["items", "(2) […]"],
    ]);
  await assertScopesForSecondColumnBreakpoint([["i", "0"]]);
  await resume(dbg);

  info(
    "We pause at the second column breakpoint, before the second loop iteration"
  );
  await waitForPaused(dbg);
  await waitForInlinePreviews(dbg);
  await assertPausedAtSourceAndLine(
    dbg,
    prettySource.id,
    LINE_INDEX_TO_BREAK_ON,
    19
  );
  await assertScopesForSecondColumnBreakpoint([["i", "1"]]);
  await resume(dbg);

  info(
    "We pause at the second column breakpoint, before we exit the loop (`items.length` is 2, so the condition will fail)"
  );
  await waitForPaused(dbg);
  await waitForInlinePreviews(dbg);
  await assertPausedAtSourceAndLine(
    dbg,
    prettySource.id,
    LINE_INDEX_TO_BREAK_ON,
    19
  );
  await assertScopesForSecondColumnBreakpoint([["i", "2"]]);
  await resume(dbg);

  info("Remove all breakpoints");
  await clickGutter(dbg, LINE_INDEX_TO_BREAK_ON);
  await waitForBreakpointCount(dbg, 0);

  ok(
    !findAllElements(dbg, "columnBreakpoints").length,
    "There is no column breakpoints anymore after clicking on the gutter"
  );
});
