/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test enabling and disabling a debugger statement using editor context menu
add_task(async function() {
  const dbg = await initDebugger("doc-pause-points.html", "pause-points.js");
  await selectSource(dbg, "pause-points.js");
  await waitForSelectedSource(dbg, "pause-points.js");

  info("Disable the first debugger statement on line 12 by gutter menu");
  rightClickElement(dbg, "gutter", 12);
  await waitForContextMenu(dbg);
  selectContextMenuItem(
    dbg,
    selectors.breakpointContextMenu.disableDbgStatement
  );
  await waitForCondition(dbg, "false");

  let bp = findBreakpoint(dbg, "pause-points.js", 12);
  is(
    bp.options.condition,
    "false",
    "The debugger statement has a conditional breakpoint with 'false' as statement"
  );

  info("Enable the previously disabled debugger statement by gutter menu");
  rightClickElement(dbg, "gutter", 12);
  await waitForContextMenu(dbg);
  selectContextMenuItem(
    dbg,
    selectors.breakpointContextMenu.enableDbgStatement
  );
  await waitForBreakpointWithoutCondition(dbg, "pause-points.js", 12, 0);

  bp = findBreakpoint(dbg, "pause-points.js", 12);
  is(bp.options.condition, null, "The conditional statement is removed");

  info("Enable the breakpoint for the second debugger statement on line 12");
  let bpElements = await waitForAllElements(dbg, "columnBreakpoints");
  ok(bpElements.length === 2, "2 column breakpoints");
  assertClass(bpElements[0], "active");
  assertClass(bpElements[1], "active", false);

  bpElements[1].click();
  await waitForBreakpointCount(dbg, 2);
  bpElements = findAllElements(dbg, "columnBreakpoints");
  assertClass(bpElements[1], "active");

  info("Disable the second debugger statement by breakpoint menu");
  rightClickEl(dbg, bpElements[1]);
  await waitForContextMenu(dbg);
  selectContextMenuItem(
    dbg,
    selectors.breakpointContextMenu.disableDbgStatement
  );
  await waitForCondition(dbg, "false");

  bp = findBreakpoints(dbg, "pause-points.js", 12)[1];
  is(
    bp.options.condition,
    "false",
    "The second debugger statement has a conditional breakpoint with 'false' as statement"
  );

  info("Enable the second debugger statement by breakpoint menu");
  bpElements = findAllElements(dbg, "columnBreakpoints");
  assertClass(bpElements[1], "has-condition");
  rightClickEl(dbg, bpElements[1]);
  await waitForContextMenu(dbg);
  selectContextMenuItem(
    dbg,
    selectors.breakpointContextMenu.enableDbgStatement
  );
  await waitForBreakpointWithoutCondition(dbg, "pause-points.js", 12, 1);

  bp = findBreakpoints(dbg, "pause-points.js", 12)[1];
  is(bp.options.condition, null, "The conditional statement is removed");
});

function waitForBreakpointWithoutCondition(dbg, url, line, index) {
  return waitForState(dbg, () => {
    const bp = findBreakpoints(dbg, url, line)[index];
    return bp && !bp.options.condition;
  });
}

function findBreakpoints(dbg, url, line) {
  const source = findSource(dbg, url);
  return dbg.selectors.getBreakpointsForSource(source.id, line);
}
