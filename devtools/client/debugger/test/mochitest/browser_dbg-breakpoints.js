/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test enabling and disabling a breakpoint using the check boxes
add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple2");

  // Create two breakpoints
  await selectSource(dbg, "simple2");
  await addBreakpoint(dbg, "simple2", 3);
  await addBreakpoint(dbg, "simple2", 5);

  // Disable the first one
  await disableBreakpoint(dbg, 0);
  let bp1 = findBreakpoint(dbg, "simple2", 3);
  let bp2 = findBreakpoint(dbg, "simple2", 5);
  is(bp1.disabled, true, "first breakpoint is disabled");
  is(bp2.disabled, false, "second breakpoint is enabled");

  // Disable and Re-Enable the second one
  await disableBreakpoint(dbg, 1);
  await enableBreakpoint(dbg, 1);
  bp2 = findBreakpoint(dbg, "simple2", 5);
  is(bp2.disabled, false, "second breakpoint is enabled");

  // Cleanup
  await cleanupBreakpoints(dbg);

  // Test enabling and disabling a breakpoint using the context menu
  await selectSource(dbg, "simple2");
  await addBreakpoint(dbg, "simple2", 3);
  await addBreakpoint(dbg, "simple2", 5);

  assertEmptyLines(dbg, [1, 2]);
  assertBreakpointSnippet(dbg, 3, "return x + y;");

  rightClickElement(dbg, "breakpointItem", 2);
  await waitForContextMenu(dbg);
  const disableBreakpointDispatch = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  selectContextMenuItem(dbg, selectors.breakpointContextMenu.disableSelf);
  await disableBreakpointDispatch;

  bp1 = findBreakpoint(dbg, "simple2", 3);
  bp2 = findBreakpoint(dbg, "simple2", 5);
  is(bp1.disabled, true, "first breakpoint is disabled");
  is(bp2.disabled, false, "second breakpoint is enabled");

  rightClickElement(dbg, "breakpointItem", 2);
  await waitForContextMenu(dbg);
  const enableBreakpointDispatch = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  selectContextMenuItem(dbg, selectors.breakpointContextMenu.enableSelf);
  await enableBreakpointDispatch;

  bp1 = findBreakpoint(dbg, "simple2", 3);
  bp2 = findBreakpoint(dbg, "simple2", 5);
  is(bp1.disabled, false, "first breakpoint is enabled");
  is(bp2.disabled, false, "second breakpoint is enabled");

  // Cleanup
  await cleanupBreakpoints(dbg);

  // Test creation of disabled breakpoint with shift-click
  await shiftClickElement(dbg, "gutter", 3);
  await waitForBreakpoint(dbg, "simple2", 3);

  const bp = findBreakpoint(dbg, "simple2", 3);
  is(bp.disabled, true, "breakpoint is disabled");

  // Cleanup
  await cleanupBreakpoints(dbg);
});

function toggleBreakpoint(dbg, index) {
  const bp = findAllElements(dbg, "breakpointItems")[index];
  const input = bp.querySelector("input");
  input.click();
}

async function disableBreakpoint(dbg, index) {
  const disabled = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  toggleBreakpoint(dbg, index);
  await disabled;
}

async function enableBreakpoint(dbg, index) {
  const enabled = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  toggleBreakpoint(dbg, index);
  await enabled;
}

async function cleanupBreakpoints(dbg) {
  clickElement(dbg, "gutter", 3);
  clickElement(dbg, "gutter", 5);
  await waitForBreakpointRemoved(dbg, "simple2", 3);
  await waitForBreakpointRemoved(dbg, "simple2", 5);
}
