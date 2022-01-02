/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests to see if we can trigger a breakpoint action via the context menu
add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple2");
  await selectSource(dbg, "simple2");
  await waitForSelectedSource(dbg, "simple2");

  await addBreakpoint(dbg, "simple2", 3);

  await openFirstBreakpointContextMenu(dbg);
  // select "Remove breakpoint"
  selectContextMenuItem(dbg, selectors.breakpointContextMenu.remove);

  await waitForState(dbg, state => dbg.selectors.getBreakpointCount() === 0);
  ok(true, "successfully removed the breakpoint");
});

// Tests "disable others", "enable others" and "remove others" context actions
add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html");
  await selectSource(dbg, "simple1");
  await waitForSelectedSource(dbg, "simple1");

  await addBreakpoint(dbg, "simple1", 4);
  await addBreakpoint(dbg, "simple1", 5);
  await addBreakpoint(dbg, "simple1", 6);

  await openFirstBreakpointContextMenu(dbg);
  // select "Disable Others"
  let dispatched = waitForDispatch(dbg.store, "SET_BREAKPOINT", 2);
  selectContextMenuItem(dbg, selectors.breakpointContextMenu.disableOthers);
  await waitForState(dbg, state =>
    dbg.selectors
      .getBreakpointsList()
      .every(bp => (bp.location.line !== 4) === bp.disabled)
  );
  await dispatched;
  ok(true, "breakpoint at 4 is the only enabled breakpoint");

  await openFirstBreakpointContextMenu(dbg);
  // select "Disable All"
  dispatched = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  selectContextMenuItem(dbg, selectors.breakpointContextMenu.disableAll);
  await waitForState(dbg, state =>
    dbg.selectors.getBreakpointsList().every(bp => bp.disabled)
  );
  await dispatched;
  ok(true, "all breakpoints are disabled");

  await openFirstBreakpointContextMenu(dbg);
  // select "Enable Others"
  dispatched = waitForDispatch(dbg.store, "SET_BREAKPOINT", 2);
  selectContextMenuItem(dbg, selectors.breakpointContextMenu.enableOthers);
  await waitForState(dbg, state =>
    dbg.selectors
      .getBreakpointsList()
      .every(bp => (bp.location.line === 4) === bp.disabled)
  );
  await dispatched;
  ok(true, "all breakpoints except line 1 are enabled");

  await openFirstBreakpointContextMenu(dbg);
  // select "Remove Others"
  dispatched = waitForDispatch(dbg.store, "REMOVE_BREAKPOINT", 2);
  selectContextMenuItem(dbg, selectors.breakpointContextMenu.removeOthers);
  await waitForState(
    dbg,
    state =>
      dbg.selectors.getBreakpointsList().length === 1 &&
      dbg.selectors.getBreakpointsList()[0].location.line === 4
  );
  await dispatched;
  ok(true, "remaining breakpoint should be on line 4");
});

async function openFirstBreakpointContextMenu(dbg) {
  rightClickElement(dbg, "breakpointItem", 2);
  await waitForContextMenu(dbg);
}
