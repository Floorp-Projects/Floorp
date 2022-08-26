/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple1.js");
  await selectSource(dbg, "long.js");

  info("1. Add a column breakpoint on line 32");
  await enableFirstBreakpoint(dbg);

  info("2. Click on the second breakpoint on line 32");
  await enableSecondBreakpoint(dbg);

  info("3. Disable second breakpoint using shift-click");
  await shiftClickDisable(dbg);

  info("4. Re-enable second breakpoint using shift-click");
  await shiftClickEnable(dbg);

  info("5. Add a condition to the first breakpoint");
  await setConditionalBreakpoint(dbg, 0, "foo");

  info("6. Add a log to the first breakpoint");
  await setLogPoint(dbg, 0, "bar");

  info("7. Disable the first breakpoint");
  await disableBreakpoint(dbg, 0);

  info("8. Remove the first breakpoint");
  await removeFirstBreakpoint(dbg);

  info("9. Add a condition to the second breakpoint");
  await setConditionalBreakpoint(dbg, 1, "foo2");

  info("10. Test removing the breakpoints by clicking in the gutter");
  await removeAllBreakpoints(dbg, 32, 0);
});

async function enableFirstBreakpoint(dbg) {
  getCM(dbg).setCursor({ line: 32, ch: 0 });
  await addBreakpoint(dbg, "long.js", 32);
  const bpMarkers = await waitForAllElements(dbg, "columnBreakpoints");

  ok(bpMarkers.length === 2, "2 column breakpoints");
  assertClass(bpMarkers[0], "active");
  assertClass(bpMarkers[1], "active", false);
}

async function enableSecondBreakpoint(dbg) {
  let bpMarkers = await waitForAllElements(dbg, "columnBreakpoints");

  bpMarkers[1].click();
  await waitForBreakpointCount(dbg, 2);

  bpMarkers = findAllElements(dbg, "columnBreakpoints");
  assertClass(bpMarkers[1], "active");
  await waitForAllElements(dbg, "breakpointItems", 2);
}

// disable active column bp with shift-click.
async function shiftClickDisable(dbg) {
  let bpMarkers = await waitForAllElements(dbg, "columnBreakpoints");
  shiftClickElement(dbg, "columnBreakpoints");
  bpMarkers = findAllElements(dbg, "columnBreakpoints");
  assertClass(bpMarkers[0], "disabled");
}

// re-enable disabled column bp with shift-click.
async function shiftClickEnable(dbg) {
  let bpMarkers = await waitForAllElements(dbg, "columnBreakpoints");
  shiftClickElement(dbg, "columnBreakpoints");
  bpMarkers = findAllElements(dbg, "columnBreakpoints");
  assertClass(bpMarkers[0], "active");
}

async function setConditionalBreakpoint(dbg, index, condition) {
  let bpMarkers = await waitForAllElements(dbg, "columnBreakpoints");
  rightClickEl(dbg, bpMarkers[index]);
  await waitForContextMenu(dbg);
  selectContextMenuItem(dbg, selectors.addConditionItem);
  await typeInPanel(dbg, condition);
  await waitForCondition(dbg, condition);

  bpMarkers = await waitForAllElements(dbg, "columnBreakpoints");
  assertClass(bpMarkers[index], "has-condition");
}

async function setLogPoint(dbg, index, expression) {
  let bpMarkers = await waitForAllElements(dbg, "columnBreakpoints");
  rightClickEl(dbg, bpMarkers[index]);
  await waitForContextMenu(dbg);

  selectContextMenuItem(dbg, selectors.addLogItem);
  await typeInPanel(dbg, expression);
  await waitForLog(dbg, expression);

  bpMarkers = await waitForAllElements(dbg, "columnBreakpoints");
  assertClass(bpMarkers[index], "has-log");
}

async function disableBreakpoint(dbg, index) {
  rightClickElement(dbg, "columnBreakpoints");
  await waitForContextMenu(dbg);
  selectContextMenuItem(dbg, selectors.disableItem);

  await waitForState(dbg, state => {
    const bp = dbg.selectors.getBreakpointsList()[index];
    return bp.disabled;
  });

  const bpMarkers = await waitForAllElements(dbg, "columnBreakpoints");
  assertClass(bpMarkers[0], "disabled");
}

async function removeFirstBreakpoint(dbg) {
  let bpMarkers = await waitForAllElements(dbg, "columnBreakpoints");

  bpMarkers[0].click();
  bpMarkers = await waitForAllElements(dbg, "columnBreakpoints");
  assertClass(bpMarkers[0], "active", false);
}

async function removeAllBreakpoints(dbg, line, count) {
  await clickGutter(dbg, 32);
  await waitForBreakpointCount(dbg, 0);

  ok(!findAllElements(dbg, "columnBreakpoints").length);
}
