/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

add_task(async function() {
  const dbg = await initDebugger("doc-pause-points.html", "pause-points.js");
  await selectSource(dbg, "pause-points.js");
  await waitForSelectedSource(dbg, "pause-points.js");

  info("Test continuing to a line");
  clickElementInTab("#sequences");
  await waitForPaused(dbg);
  await waitForInlinePreviews(dbg);

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
});

async function continueToLine(dbg, line) {
  rightClickElement(dbg, "gutter", line);
  await waitForContextMenu(dbg);
  selectContextMenuItem(dbg, selectors.editorContextMenu.continueToHere);
  await waitForDispatch(dbg.store, "RESUME");
  await waitForPaused(dbg);
  await waitForInlinePreviews(dbg);
}

async function continueToColumn(dbg, pos) {
  await rightClickAtPos(dbg, pos);
  await waitForContextMenu(dbg);

  selectContextMenuItem(dbg, selectors.editorContextMenu.continueToHere);
  await waitForDispatch(dbg.store, "RESUME");
  await waitForPaused(dbg);
  await waitForInlinePreviews(dbg);
}
