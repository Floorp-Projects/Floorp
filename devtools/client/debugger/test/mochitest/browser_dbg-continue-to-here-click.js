/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

async function cmdClickLine(dbg, line) {
  await cmdClickGutter(dbg, line);
}

add_task(async function() {
  const dbg = await initDebugger("doc-pause-points.html", "pause-points.js");
  await selectSource(dbg, "pause-points.js");
  await waitForSelectedSource(dbg, "pause-points.js");

  info("Test cmd+click continueing to a line");
  clickElementInTab("#sequences");
  await waitForPaused(dbg);
  await waitForInlinePreviews(dbg);
  await cmdClickLine(dbg, 31);
  await waitForPausedLine(dbg, 31);
  assertDebugLine(dbg, 31, 4);
  await resume(dbg);
  await waitForRequestsToSettle(dbg);
});
