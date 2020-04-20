/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

async function continueToLine(dbg, line) {
  rightClickElement(dbg, "gutter", line);
  selectContextMenuItem(dbg, selectors.editorContextMenu.continueToHere);
  return waitForPause(dbg);
}

async function continueToColumn(dbg, pos) {
  await rightClickAtPos(dbg, pos);
  selectContextMenuItem(dbg, selectors.editorContextMenu.continueToHere);
  await waitForPause(dbg);
}

async function cmdClickLine(dbg, line) {
  await cmdClickGutter(dbg, line);
  return waitForPause(dbg);
}

async function waitForPause(dbg) {
  await waitForDispatch(dbg, "RESUME");
  await waitForPaused(dbg);
  await waitForInlinePreviews(dbg);
}

add_task(async function() {
  const dbg = await initDebugger("doc-pause-points.html", "pause-points.js");
  await selectSource(dbg, "pause-points.js");
  await waitForSelectedSource(dbg, "pause-points.js");

  info("Test continuing to a line");
  clickElementInTab("#sequences");
  await waitForPaused(dbg);
  await continueToLine(dbg, 31);
  assertDebugLine(dbg, 31, 4);
  await resume(dbg);

  info("Test continuing to a column");
  clickElementInTab("#sequences");
  await waitForPaused(dbg);
  await waitForInlinePreviews(dbg);
  await continueToColumn(dbg, { line: 31, ch: 7 });
  assertDebugLine(dbg, 31, 4);
  await resume(dbg);

  info("Test cmd+click continueing to a line");
  clickElementInTab("#sequences");
  await waitForPaused(dbg);
  await waitForInlinePreviews(dbg);
  await cmdClickLine(dbg, 31);
  assertDebugLine(dbg, 31, 4);
  await resume(dbg);
});
