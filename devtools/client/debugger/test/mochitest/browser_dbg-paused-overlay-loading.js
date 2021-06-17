/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that the paused overlay isn't visible after resuming if the debugger paused
// while the DOM was still loading (Bug 1678636).

add_task(async function() {
  const dbg = await initDebuggerWithAbsoluteURL(
    "data:text/html,<meta charset=utf8><script>debugger;</script>"
  );

  info("Reload the page to hit the debugger statement while loading");
  reload(dbg);
  await waitForPaused(dbg);
  // TODO: Check that the overlay is displayed (Bug 1580394), it's not at the moment.
  ok(true, "We're paused");

  // TODO: Resume with clicking on the overlay button, when Bug 1580394 is fixed.
  info("Resume");
  await resume(dbg);
  ok(true, "We're not paused anymore");

  info("Wait for a bit, just to make sure the overlay isn't displayed");
  await waitForTime(5000);

  const highlighterTestFront = await getHighlighterTestFront(dbg.toolbox);
  const isPausedOverlayVisible = await highlighterTestFront.isPausedDebuggerOverlayVisible();
  is(
    isPausedOverlayVisible,
    false,
    "The pause overlay is not visible after resuming"
  );
});
