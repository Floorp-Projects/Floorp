/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests the breakpoint gutter and making sure breakpoint icons exist
// correctly

// FIXME bug 1524374 removing breakpoints in this test can cause uncaught
// rejections and make bug 1512742 permafail.
PromiseTestUtils.allowMatchingRejectionsGlobally(/NS_ERROR_NOT_INITIALIZED/);

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple1.js");
  const { getState } = dbg;
  const source = findSource(dbg, "simple1.js");

  await selectSource(dbg, source.url);

  // Make sure that clicking the gutter creates a breakpoint icon.
  clickGutter(dbg, 4);
  await waitForDispatch(dbg.store, "SET_BREAKPOINT");
  is(dbg.selectors.getBreakpointCount(), 1, "One breakpoint exists");
  await assertBreakpoint(dbg, 4);

  // Make sure clicking at the same place removes the icon.
  clickGutter(dbg, 4);
  await waitForDispatch(dbg.store, "REMOVE_BREAKPOINT");
  is(dbg.selectors.getBreakpointCount(), 0, "No breakpoints exist");
  await assertNoBreakpoint(dbg, 4);
});

add_task(async function() {
  info("Ensure clicking on gutter to add breakpoint will un-blackbox source");
  const dbg = await initDebugger("doc-sourcemaps3.html");
  dbg.actions.toggleMapScopes();
  const {
    selectors: { getBreakpoint, getBreakpointCount },
    getState
  } = dbg;
  await waitForSources(dbg, "bundle.js", "sorted.js", "test.js");

  info("blackbox the source");
  const sortedSrc = findSource(dbg, "sorted.js");
  await selectSource(dbg, sortedSrc);
  await clickElement(dbg, "blackbox");
  await waitForDispatch(dbg.store, "BLACKBOX");

  // invoke test
  invokeInTab("test");
  // should not pause
  is(isPaused(dbg), false);

  info("ensure gutter breakpoint gets set with click");
  clickGutter(dbg, 4);
  await waitForDispatch(dbg.store, "SET_BREAKPOINT");
  is(dbg.selectors.getBreakpointCount(), 1, "One breakpoint exists");
  await assertBreakpoint(dbg, 4);

  // click on test
  invokeInTab("test");
  // verify pause at breakpoint.
  await waitForPaused(dbg);
  ok(true, "source is un-blackboxed");
});

// Utilities for interacting with the editor
function clickGutter(dbg, line) {
  clickElement(dbg, "gutter", line);
}
