/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test cmd+click continuing to a line

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-pause-points.html", "pause-points.js");
  await selectSource(dbg, "pause-points.js");
  await waitForSelectedSource(dbg, "pause-points.js");

  info(
    "Pause the debugger by clicking a button with a click handler containing a debugger statement"
  );
  clickElementInTab("#sequences");
  await waitForPaused(dbg);
  await waitForInlinePreviews(dbg);
  ok(true, "Debugger is paused");

  info("Cmd+click on a line and check the debugger continues to that line");
  const lineToContinueTo = 31;
  const onResumed = waitForResumed(dbg);
  await cmdClickLine(dbg, lineToContinueTo);

  // continuing will resume and pause again. Let's wait until we resume so we can properly
  // wait for the next pause.
  await onResumed;
  // waitForPaused properly waits for the scopes to be available
  await waitForPaused(dbg);

  assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "pause-points.js").id,
    lineToContinueTo,
    5
  );
  ok(true, "Debugger continued to the expected line");

  info("Resume");
  await resume(dbg);
  await waitForRequestsToSettle(dbg);
});

async function cmdClickLine(dbg, line) {
  await cmdClickGutter(dbg, line);
}
