/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests the paused overlay

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-scripts.html");

  // Sanity check
  const highlighterTestFront = await getHighlighterTestFront(dbg.toolbox);
  let isPausedOverlayVisible =
    await highlighterTestFront.isPausedDebuggerOverlayVisible();
  is(
    isPausedOverlayVisible,
    false,
    "The pause overlay is not visible in the beginning"
  );

  info("Create an eval script that pauses itself.");
  invokeInTab("doEval");
  await waitForPaused(dbg);

  info("Check that the paused overlay is displayed");
  await waitFor(() => highlighterTestFront.isPausedDebuggerOverlayVisible());
  ok(true, "Paused debugger overlay is visible");

  const pauseLine = getVisibleSelectedFrameLine(dbg);
  is(pauseLine, 2, "We're paused at the expected location");

  info("Test clicking the step over button");
  await highlighterTestFront.clickPausedDebuggerOverlayButton(
    "paused-dbg-step-button"
  );
  await waitFor(() => isPaused(dbg) && getVisibleSelectedFrameLine(dbg) === 4);
  ok(true, "We're paused at the expected location after stepping");

  isPausedOverlayVisible =
    await highlighterTestFront.isPausedDebuggerOverlayVisible();
  is(isPausedOverlayVisible, true, "The pause overlay is still visible");

  info("Test clicking the highlighter resume button");
  await highlighterTestFront.clickPausedDebuggerOverlayButton(
    "paused-dbg-resume-button"
  );

  await waitForResumed(dbg);
  ok("The debugger isn't paused after clicking on the resume button");

  await waitFor(async () => {
    const visible = await highlighterTestFront.isPausedDebuggerOverlayVisible();
    return !visible;
  });
  ok(true, "The overlay is now hidden");

  info(
    "Check that the highlighter is removed when clicking on the debugger resume button"
  );
  invokeInTab("doEval");
  await waitFor(() => highlighterTestFront.isPausedDebuggerOverlayVisible());
  ok(true, "Paused debugger overlay is visible again");

  info("Click debugger UI resume button");
  const resumeButton = await waitFor(() => findElement(dbg, "resume"));
  resumeButton.click();
  await waitFor(async () => {
    const visible = await highlighterTestFront.isPausedDebuggerOverlayVisible();
    return !visible;
  });
  ok(true, "The overlay is hidden after clicking on the resume button");
});
