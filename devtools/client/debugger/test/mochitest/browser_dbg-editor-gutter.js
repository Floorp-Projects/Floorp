/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests the breakpoint gutter and making sure breakpoint icons exist
// correctly

// FIXME bug 1524374 removing breakpoints in this test can cause uncaught
// rejections and make bug 1512742 permafail.
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.whitelistRejectionsGlobally(/NS_ERROR_NOT_INITIALIZED/);

// Utilities for interacting with the editor
function clickGutter(dbg, line) {
  clickElement(dbg, "gutter", line);
}

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple1.js");
  const { getState } = dbg;
  const source = findSource(dbg, "simple1.js");

  await selectSource(dbg, source.url);

  // Make sure that clicking the gutter creates a breakpoint icon.
  clickGutter(dbg, 4);
  await waitForDispatch(dbg, "SET_BREAKPOINT");
  is(dbg.selectors.getBreakpointCount(), 1, "One breakpoint exists");
  await assertEditorBreakpoint(dbg, 4, true);

  // Make sure clicking at the same place removes the icon.
  clickGutter(dbg, 4);
  await waitForDispatch(dbg, "REMOVE_BREAKPOINT");
  is(dbg.selectors.getBreakpointCount(), 0, "No breakpoints exist");
  await assertEditorBreakpoint(dbg, 4, false);
});
