/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the breakpoint gutter and making sure breakpoint icons exist
// correctly

// FIXME bug 1524374 removing breakpoints in this test can cause uncaught
// rejections and make bug 1512742 permafail.
const { PromiseTestUtils } = scopedCuImport(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.whitelistRejectionsGlobally(/NS_ERROR_NOT_INITIALIZED/);

// Utilities for interacting with the editor
function clickGutter(dbg, line) {
  clickElement(dbg, "gutter", line);
}

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple1.js");
  const {
    selectors: { getBreakpoint, getBreakpointCount },
    getState
  } = dbg;
  const source = findSource(dbg, "simple1.js");

  await selectSource(dbg, source.url);

  // Make sure that clicking the gutter creates a breakpoint icon.
  clickGutter(dbg, 4);
  await waitForDispatch(dbg, "ADD_BREAKPOINT");
  is(getBreakpointCount(getState()), 1, "One breakpoint exists");
  assertEditorBreakpoint(dbg, 4, true);

  // Make sure clicking at the same place removes the icon.
  clickGutter(dbg, 4);
  await waitForDispatch(dbg, "REMOVE_BREAKPOINT");
  is(getBreakpointCount(getState()), 0, "No breakpoints exist");
  assertEditorBreakpoint(dbg, 4, false);
});
