/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

async function enableFirstBreakpoint(dbg) {
  getCM(dbg).setCursor({ line: 32, ch: 0 });
  await addBreakpoint(dbg, "long", 32);
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

async function setConditionalBreakpoint(dbg, index, condition) {
  let bpMarkers = await waitForAllElements(dbg, "columnBreakpoints");
  rightClickEl(dbg, bpMarkers[index]);
  selectContextMenuItem(dbg, selectors.addConditionItem);
  await typeInPanel(dbg, condition);
  await waitForCondition(dbg, condition);

  bpMarkers = await waitForAllElements(dbg, "columnBreakpoints");
  assertClass(bpMarkers[index], "has-condition");
}

async function setLogPoint(dbg, index, expression) {
  let bpMarkers = await waitForAllElements(dbg, "columnBreakpoints");
  rightClickEl(dbg, bpMarkers[index]);

  selectContextMenuItem(dbg, selectors.addLogItem);
  await typeInPanel(dbg, expression);
  await waitForLog(dbg, expression);

  bpMarkers = await waitForAllElements(dbg, "columnBreakpoints");
  assertClass(bpMarkers[index], "has-log");
}

async function disableBreakpoint(dbg, index) {
  rightClickElement(dbg, "columnBreakpoints");
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
  clickGutter(dbg, 32);
  await waitForBreakpointCount(dbg, 0);

  ok(findAllElements(dbg, "columnBreakpoints").length == 0);
}

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple1");
  await selectSource(dbg, "long");

  info("1. Add a column breakpoint on line 32");
  await enableFirstBreakpoint(dbg);

  info("2. Click on the second breakpoint on line 32");
  await enableSecondBreakpoint(dbg);

  info("3. Add a condition to the first breakpoint");
  await setConditionalBreakpoint(dbg, 0, "foo");

  info("4. Add a log to the first breakpoint");
  await setLogPoint(dbg, 0, "bar");

  info("5. Disable the first breakpoint");
  await disableBreakpoint(dbg, 0);

  info("6. Remove the first breakpoint");
  await removeFirstBreakpoint(dbg);

  info("7. Add a condition to the second breakpoint");
  await setConditionalBreakpoint(dbg, 1, "foo2");

  info("8. test removing the breakpoints by clicking in the gutter");
  await removeAllBreakpoints(dbg, 32, 0);
});
