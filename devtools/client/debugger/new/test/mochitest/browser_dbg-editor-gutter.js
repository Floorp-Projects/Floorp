/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the breakpoint gutter and making sure breakpoint icons exist
// correctly

// Utilities for interacting with the editor
function clickGutter(dbg, line) {
  clickElement(dbg, "gutter", line);
}

function getLineEl(dbg, line) {
  const lines = dbg.win.document.querySelectorAll(".CodeMirror-code > div");
  return lines[line - 1];
}

function assertEditorBreakpoint(dbg, line, shouldExist) {
  const exists = !!getLineEl(dbg, line).querySelector(".new-breakpoint");
  ok(
    exists === shouldExist,
    "Breakpoint " +
      (shouldExist ? "exists" : "does not exist") +
      " on line " +
      line
  );
}

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html");
  const {
    selectors: { getBreakpoints, getBreakpoint },
    getState
  } = dbg;
  const source = findSource(dbg, "simple1.js");

  await selectSource(dbg, source.url);

  // Make sure that clicking the gutter creates a breakpoint icon.
  clickGutter(dbg, 4);
  await waitForDispatch(dbg, "ADD_BREAKPOINT");
  is(getBreakpoints(getState()).size, 1, "One breakpoint exists");
  assertEditorBreakpoint(dbg, 4, true);

  // Make sure clicking at the same place removes the icon.
  clickGutter(dbg, 4);
  await waitForDispatch(dbg, "REMOVE_BREAKPOINT");
  is(getBreakpoints(getState()).size, 0, "No breakpoints exist");
  assertEditorBreakpoint(dbg, 4, false);

  // Ensure that clicking the gutter removes all breakpoints on a given line
  await addBreakpoint(dbg, source, 4, 0);
  await addBreakpoint(dbg, source, 4, 1);
  await addBreakpoint(dbg, source, 4, 2);
  clickGutter(dbg, 4);
  await waitForState(dbg, state => dbg.selectors.getBreakpoints(state).size == 0);
  assertEditorBreakpoint(dbg, 4, false);
});
