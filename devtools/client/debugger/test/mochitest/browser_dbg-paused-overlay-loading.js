/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that the paused overlay isn't visible after resuming if the debugger paused
// while the DOM was still loading (Bug 1678636).

"use strict";

add_task(async function () {
  const dbg = await initDebuggerWithAbsoluteURL(
    "data:text/html,<meta charset=utf8><script>debugger;</script>"
  );

  info("Reload the page to hit the debugger statement while loading");
  const onReloaded = reload(dbg);
  await waitForPaused(dbg);
  ok(true, "We're paused");

  info("Check that the paused overlay is displayed");
  const highlighterTestFront = await dbg.toolbox.target.getFront(
    "highlighterTest"
  );

  await waitFor(async () => {
    const visible = await highlighterTestFront.isPausedDebuggerOverlayVisible();
    return visible;
  });
  ok(true, "Paused debugger overlay is visible");

  info("Click the resume button");
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

  info("Wait for reload to complete after resume");
  await onReloaded;
});
