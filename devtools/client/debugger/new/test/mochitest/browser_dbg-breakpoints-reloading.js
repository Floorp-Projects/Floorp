/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests pending breakpoints when reloading

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
  return waitForDispatch(dbg, "ADD_BREAKPOINT");
}

function assertEditorBreakpoint(dbg, line) {
  const exists = !!getLineEl(dbg, line).querySelector(".new-breakpoint");
  ok(exists, `Breakpoint exists on line ${line}`);
}

add_task(function*() {
  const dbg = yield initDebugger("doc-scripts.html");
  const { selectors: { getBreakpoints, getBreakpoint }, getState } = dbg;
  const source = findSource(dbg, "simple1.js");

  yield selectSource(dbg, source.url);
  yield addBreakpoint(dbg, 5);
  yield addBreakpoint(dbg, 2);

  yield reload(dbg, "simple1");
  yield waitForSelectedSource(dbg);
  assertEditorBreakpoint(dbg, 4);
  assertEditorBreakpoint(dbg, 5);
});
