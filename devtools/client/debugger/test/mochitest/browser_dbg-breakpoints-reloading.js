/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests pending breakpoints when reloading
requestLongerTimeout(3);

// Utilities for interacting with the editor
function clickGutter(dbg, line) {
  clickElement(dbg, "gutter", line);
}

function getLineEl(dbg, line) {
  const lines = dbg.win.document.querySelectorAll(".CodeMirror-code > div");
  return lines[line - 1];
}

function addBreakpoint(dbg, line) {
  clickGutter(dbg, line);
  return waitForDispatch(dbg, "SET_BREAKPOINT");
}

function assertEditorBreakpoint(dbg, line) {
  const lineEl = getLineEl(dbg, line);
  const exists = lineEl.classList.contains("new-breakpoint");
  ok(exists, `Breakpoint exists on line ${line}`);
}

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple1.js");
  const source = findSource(dbg, "simple1.js");

  await selectSource(dbg, source.url);
  await addBreakpoint(dbg, 5);
  await addBreakpoint(dbg, 4);

  const syncedBps = waitForDispatch(dbg, "SET_BREAKPOINT", 2);
  await reload(dbg, "simple1");
  await waitForSelectedSource(dbg, "simple1");
  await syncedBps;

  assertEditorBreakpoint(dbg, 4);
  assertEditorBreakpoint(dbg, 5);
});
