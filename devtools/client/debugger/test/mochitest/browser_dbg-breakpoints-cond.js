/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple2.js");
  await pushPref("devtools.debugger.features.column-breakpoints", true);

  await selectSource(dbg, "simple2.js");
  await waitForSelectedSource(dbg, "simple2.js");

  info("Set condition `1`");
  await setConditionalBreakpoint(dbg, 5, "1");
  await waitForCondition(dbg, 1);

  let bp = findBreakpoint(dbg, "simple2.js", 5);
  is(bp.options.condition, "1", "breakpoint is created with the condition");
  await assertConditionBreakpoint(dbg, 5);

  info("Edit the conditional breakpoint set above");
  await setConditionalBreakpoint(dbg, 5, "2");
  await waitForCondition(dbg, 12);

  bp = findBreakpoint(dbg, "simple2.js", 5);
  is(bp.options.condition, "12", "breakpoint is created with the condition");
  await assertConditionBreakpoint(dbg, 5);

  info("Hit 'Enter' when the cursor is in the conditional statement");
  rightClickElement(dbg, "gutter", 5);
  await waitForContextMenu(dbg);
  selectContextMenuItem(dbg, `${selectors.editConditionItem}`);
  await waitForConditionalPanelFocus(dbg);
  pressKey(dbg, "Left");
  pressKey(dbg, "Enter");
  await waitForCondition(dbg, 12);

  bp = findBreakpoint(dbg, "simple2.js", 5);
  is(bp.options.condition, "12", "Hit 'Enter' doesn't add a new line");

  info("Hit 'Alt+Enter' when the cursor is in the conditional statement");
  rightClickElement(dbg, "gutter", 5);
  await waitForContextMenu(dbg);
  selectContextMenuItem(dbg, `${selectors.editConditionItem}`);
  await waitForConditionalPanelFocus(dbg);
  pressKey(dbg, "Left");
  pressKey(dbg, "AltEnter");
  pressKey(dbg, "Enter");
  await waitForCondition(dbg, "1\n2");

  bp = findBreakpoint(dbg, "simple2.js", 5);
  is(bp.options.condition, "1\n2", "Hit 'Alt+Enter' adds a new line");

  clickElement(dbg, "gutter", 5);
  await waitForDispatch(dbg.store, "REMOVE_BREAKPOINT");
  bp = findBreakpoint(dbg, "simple2.js", 5);
  is(bp, undefined, "breakpoint was removed");
  await assertNoBreakpoint(dbg, 5);

  info("Adding a condition to a breakpoint");
  clickElement(dbg, "gutter", 5);
  await waitForDispatch(dbg.store, "SET_BREAKPOINT");
  await setConditionalBreakpoint(dbg, 5, "1");
  await waitForCondition(dbg, 1);

  bp = findBreakpoint(dbg, "simple2.js", 5);
  is(bp.options.condition, "1", "breakpoint is created with the condition");
  await assertConditionBreakpoint(dbg, 5);

  info("Double click the conditional breakpoint in secondary pane");
  dblClickElement(dbg, "conditionalBreakpointInSecPane");
  is(
    dbg.win.document.activeElement.tagName,
    "TEXTAREA",
    "The textarea of conditional breakpoint panel is focused"
  );

  info("Click the conditional breakpoint in secondary pane");
  await clickElement(dbg, "conditionalBreakpointInSecPane");
  const conditonalPanel = findElement(dbg, "conditionalPanel");
  is(conditonalPanel, null, "The conditional breakpoint panel is closed");

  rightClickElement(dbg, "breakpointItem", 2);
  await waitForContextMenu(dbg);
  info('select "remove condition"');
  selectContextMenuItem(dbg, selectors.breakpointContextMenu.removeCondition);
  await waitForBreakpointWithoutCondition(dbg, "simple2.js", 5);
  bp = findBreakpoint(dbg, "simple2.js", 5);
  is(bp.options.condition, null, "breakpoint condition removed");

  info('Add "log point"');
  await setLogPoint(dbg, 5, "44");
  await waitForLog(dbg, 44);
  await assertLogBreakpoint(dbg, 5);

  bp = findBreakpoint(dbg, "simple2.js", 5);
  is(bp.options.logValue, "44", "breakpoint condition removed");

  await altClickElement(dbg, "gutter", 6);
  bp = await waitForBreakpoint(dbg, "simple2.js", 6);
  is(bp.options.logValue, "displayName", "logPoint has default value");

  info("Double click the logpoint in secondary pane");
  dblClickElement(dbg, "logPointInSecPane");
  is(
    dbg.win.document.activeElement.tagName,
    "TEXTAREA",
    "The textarea of logpoint panel is focused"
  );

  info("Click the logpoint in secondary pane");
  await clickElement(dbg, "logPointInSecPane");
  const logPointPanel = findElement(dbg, "logPointPanel");
  is(logPointPanel, null, "The logpoint panel is closed");
});

function waitForBreakpointWithoutCondition(dbg, url, line) {
  return waitForState(dbg, () => {
    const bp = findBreakpoint(dbg, url, line);
    return bp && !bp.options.condition;
  });
}

async function setConditionalBreakpoint(dbg, index, condition) {
  // Make this work with either add or edit menu items
  const { addConditionItem, editConditionItem } = selectors;
  const selector = `${addConditionItem},${editConditionItem}`;
  rightClickElement(dbg, "gutter", index);
  await waitForContextMenu(dbg);
  selectContextMenuItem(dbg, selector);
  typeInPanel(dbg, condition);
}

async function waitForConditionalPanelFocus(dbg) {
  await waitFor(() => dbg.win.document.activeElement.tagName === "TEXTAREA");
}
