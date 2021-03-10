/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests pending breakpoints when reloading
requestLongerTimeout(3);

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple1.js");
  const source = findSource(dbg, "simple1.js");

  await selectSource(dbg, source.url);
  await addBreakpointViaGutter(dbg, 5);
  await addBreakpointViaGutter(dbg, 4);

  const syncedBps = waitForDispatch(dbg.store, "SET_BREAKPOINT", 2);
  await reload(dbg, "simple1");
  await waitForSelectedSource(dbg, "simple1");
  await syncedBps;

  await assertBreakpoint(dbg, 4);
  await assertBreakpoint(dbg, 5);
});

// Test that pending breakpoints are installed in inline scripts as they are
// sent to the client.
add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "doc-scripts.html");
  let source = findSource(dbg, "doc-scripts.html");

  await selectSource(dbg, source.url);
  await addBreakpoint(dbg, "doc-scripts.html", 22);
  await addBreakpoint(dbg, "doc-scripts.html", 27);

  await reload(dbg, "doc-scripts.html");
  source = findSource(dbg, "doc-scripts.html");

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 22);
  await assertBreakpoint(dbg, 22);

  // The second breakpoint we added is in a later inline script, and won't
  // appear until after we have resumed from the first breakpoint and the
  // second inline script has been created.
  await resume(dbg);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 27);
  await assertBreakpoint(dbg, 27);
});

// Utilities for interacting with the editor
function clickGutter(dbg, line) {
  clickElement(dbg, "gutter", line);
}

function getLineEl(dbg, line) {
  const lines = dbg.win.document.querySelectorAll(".CodeMirror-code > div");
  return lines[line - 1];
}

function addBreakpointViaGutter(dbg, line) {
  clickGutter(dbg, line);
  return waitForDispatch(dbg.store, "SET_BREAKPOINT");
}
