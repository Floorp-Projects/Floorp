/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests the breakpoint gutter and making sure breakpoint icons exist
// correctly

"use strict";

// FIXME bug 1524374 removing breakpoints in this test can cause uncaught
// rejections and make bug 1512742 permafail.
PromiseTestUtils.allowMatchingRejectionsGlobally(/NS_ERROR_NOT_INITIALIZED/);

add_task(async function testGutterBreakpoints() {
  const dbg = await initDebugger("doc-scripts.html", "simple1.js");
  const source = findSource(dbg, "simple1.js");

  await selectSource(dbg, source);

  // Make sure that clicking the gutter creates a breakpoint icon.
  await clickGutter(dbg, 4);
  await waitForDispatch(dbg.store, "SET_BREAKPOINT");
  is(dbg.selectors.getBreakpointCount(), 1, "One breakpoint exists");
  await assertBreakpoint(dbg, 4);

  // Make sure clicking at the same place removes the icon.
  await clickGutter(dbg, 4);
  await waitForDispatch(dbg.store, "REMOVE_BREAKPOINT");
  is(dbg.selectors.getBreakpointCount(), 0, "No breakpoints exist");
  await assertNoBreakpoint(dbg, 4);
});

add_task(async function testGutterBreakpointsInIgnoredSource() {
  await pushPref("devtools.debugger.map-scopes-enabled", true);
  info(
    "Ensure clicking on gutter to add breakpoint should not un-ignore source"
  );
  const dbg = await initDebugger("doc-sourcemaps3.html");
  await waitForSources(dbg, "bundle.js", "sorted.js", "test.js");

  info("Ignore the source");
  await selectSource(dbg, findSource(dbg, "sorted.js"));
  await clickElement(dbg, "blackbox");
  await waitForDispatch(dbg.store, "BLACKBOX_WHOLE_SOURCES");

  // invoke test
  invokeInTab("test");

  // wait to make sure no pause has occured
  await wait(1000);
  assertNotPaused(dbg);

  info("Ensure gutter breakpoint gets set with click");
  await clickGutter(dbg, 4);
  await waitForDispatch(dbg.store, "SET_BREAKPOINT");

  info("Assert that the `Enable breakpoint` context menu item is disabled");
  const popup = await openContextMenuInDebugger(dbg, "gutter", 4);
  await assertContextMenuItemDisabled(
    dbg,
    "#node-menu-enable-breakpoint",
    true
  );
  await closeContextMenu(dbg, popup);

  is(dbg.selectors.getBreakpointCount(), 1, "One breakpoint exists");
  await assertBreakpoint(dbg, 4);
  is(
    findBreakpoint(dbg, "sorted.js", 4).disabled,
    true,
    "The breakpoint in the ignored source looks disabled"
  );

  invokeInTab("test");

  await wait(1000);
  assertNotPaused(dbg);

  ok(true, "source is still blackboxed");
});

add_task(async function testGutterBreakpointsForSourceWithIgnoredLines() {
  info(
    "Asserts that adding a breakpoint to the gutter of an un-ignored line does not un-ignore other ranges in the source"
  );
  const dbg = await initDebugger("doc-sourcemaps3.html");
  await waitForSources(dbg, "bundle.js", "sorted.js", "test.js");

  await selectSource(dbg, findSource(dbg, "sorted.js"));

  info("Ignoring line 17 to 21 using the editor context menu");
  await selectEditorLinesAndOpenContextMenu(dbg, {
    startLine: 17,
    endLine: 21,
  });
  await selectBlackBoxContextMenuItem(dbg, "blackbox-lines");

  info("Set breakpoint on an un-ignored line");
  await clickGutter(dbg, 4);
  await waitForDispatch(dbg.store, "SET_BREAKPOINT");

  info("Assert that the `Disable breakpoint` context menu item is enabled");
  const popup = await openContextMenuInDebugger(dbg, "gutter", 4);
  await assertContextMenuItemDisabled(
    dbg,
    "#node-menu-disable-breakpoint",
    false
  );
  await closeContextMenu(dbg, popup);

  info("Assert that the lines 17 to 21 are still ignored");
  assertIgnoredStyleInSourceLines(dbg, {
    lines: [17, 21],
    hasBlackboxedLinesClass: true,
  });

  info(
    "Asserts that adding a breakpoint to the gutter of an ignored line creates a disabled breakpoint"
  );
  info("Set breakpoint on an ignored line");
  await clickGutter(dbg, 19);
  await waitForDispatch(dbg.store, "SET_BREAKPOINT");

  info("Assert that the `Enable breakpoint` context menu item is disabled");
  const popup2 = await openContextMenuInDebugger(dbg, "gutter", 19);
  await assertContextMenuItemDisabled(
    dbg,
    "#node-menu-enable-breakpoint",
    true
  );
  await closeContextMenu(dbg, popup2);

  info("Assert that the breakpoint on the line 19 is visually disabled");
  is(
    findBreakpoint(dbg, "sorted.js", 19).disabled,
    true,
    "The breakpoint on an ignored line is disabled"
  );
});

async function assertContextMenuItemDisabled(dbg, selector, expectedState) {
  const item = await waitFor(() => findContextMenu(dbg, selector));
  is(item.disabled, expectedState, "The context menu item is disabled");
}
