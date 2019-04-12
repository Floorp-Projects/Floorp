/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

function openFirstBreakpointContextMenu(dbg) {
  rightClickElement(dbg, "breakpointItem", 2);
}

// Tests to see if we can trigger a breakpoint action via the context menu
add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple2");
  await selectSource(dbg, "simple2");
  await waitForSelectedSource(dbg, "simple2");

  await addBreakpoint(dbg, "simple2", 3);

  openFirstBreakpointContextMenu(dbg);
  // select "Remove breakpoint"
  selectContextMenuItem(dbg, selectors.breakpointContextMenu.remove);

  await waitForState(dbg, state => dbg.selectors.getBreakpointCount() === 0);
  ok("successfully removed the breakpoint");
});

// Tests "disable others", "enable others" and "remove others" context actions
add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html");
  await selectSource(dbg, "simple1");
  await waitForSelectedSource(dbg, "simple1");

  await addBreakpoint(dbg, "simple1", 4);
  await addBreakpoint(dbg, "simple1", 5);
  await addBreakpoint(dbg, "simple1", 6);

  openFirstBreakpointContextMenu(dbg);
  // select "Disable Others"
  // FIXME bug 1524374 this waitForDispatch call only sees one dispatch for
  // SET_BREAKPOINT even though three are triggered, due to the order in
  // which promises get resolved. The problem seems to indicate a coverage gap
  // in waitUntilService(). Workaround this by only waiting for one dispatch,
  // though this is fragile and could break again in the future.
  let dispatched = waitForDispatch(dbg, "SET_BREAKPOINT", /* 2*/ 1);
  selectContextMenuItem(dbg, selectors.breakpointContextMenu.disableOthers);
  await waitForState(dbg, state =>
    dbg.selectors
      .getBreakpointsList()
      .every(bp => (bp.location.line !== 4) === bp.disabled)
  );
  await dispatched;
  ok("breakpoint at 4 is the only enabled breakpoint");

  openFirstBreakpointContextMenu(dbg);
  // select "Disable All"
  dispatched = waitForDispatch(dbg, "SET_BREAKPOINT");
  selectContextMenuItem(dbg, selectors.breakpointContextMenu.disableAll);
  await waitForState(dbg, state =>
    dbg.selectors.getBreakpointsList().every(bp => bp.disabled)
  );
  await dispatched;
  ok("all breakpoints are disabled");

  openFirstBreakpointContextMenu(dbg);
  // select "Enable Others"
  dispatched = waitForDispatch(dbg, "SET_BREAKPOINT", 2);
  selectContextMenuItem(dbg, selectors.breakpointContextMenu.enableOthers);
  await waitForState(dbg, state =>
    dbg.selectors
      .getBreakpointsList()
      .every(bp => (bp.location.line === 4) === bp.disabled)
  );
  await dispatched;
  ok("all breakpoints except line 1 are enabled");

  openFirstBreakpointContextMenu(dbg);
  // select "Remove Others"
  dispatched = waitForDispatch(dbg, "REMOVE_BREAKPOINT", 2);
  selectContextMenuItem(dbg, selectors.breakpointContextMenu.removeOthers);
  await waitForState(
    dbg,
    state =>
      dbg.selectors.getBreakpointsList().length === 1 &&
      dbg.selectors.getBreakpointsList()[0].location.line === 4
  );
  await dispatched;
  ok("remaining breakpoint should be on line 4");
});
