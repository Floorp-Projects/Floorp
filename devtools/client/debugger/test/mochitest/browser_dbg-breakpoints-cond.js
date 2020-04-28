/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple2");
  await pushPref("devtools.debugger.features.column-breakpoints", true);

  await selectSource(dbg, "simple2");
  await waitForSelectedSource(dbg, "simple2");

  info("Set condition `1`");
  await setConditionalBreakpoint(dbg, 5, "1");
  await waitForCondition(dbg, 1);

  let bp = findBreakpoint(dbg, "simple2", 5);
  is(bp.options.condition, "1", "breakpoint is created with the condition");
  await assertEditorBreakpoint(dbg, 5, { hasCondition: true });

  info("Edit the conditional breakpoint set above");
  await setConditionalBreakpoint(dbg, 5, "2");
  await waitForCondition(dbg, 12);

  bp = findBreakpoint(dbg, "simple2", 5);
  is(bp.options.condition, "12", "breakpoint is created with the condition");
  await assertEditorBreakpoint(dbg, 5, { hasCondition: true });

  info("Hit 'Enter' when the cursor is in the conditional statement");
  rightClickElement(dbg, "gutter", 5);
  selectContextMenuItem(dbg, `${selectors.editConditionItem}`);
  await waitForConditionalPanelFocus(dbg);
  pressKey(dbg, "Left");
  pressKey(dbg, "Enter");
  await waitForCondition(dbg, 12);

  bp = findBreakpoint(dbg, "simple2", 5);
  is(bp.options.condition, "12", "Hit 'Enter' doesn't add a new line");

  info("Hit 'Alt+Enter' when the cursor is in the conditional statement");
  rightClickElement(dbg, "gutter", 5);
  selectContextMenuItem(dbg, `${selectors.editConditionItem}`);
  await waitForConditionalPanelFocus(dbg);
  pressKey(dbg, "Left");
  pressKey(dbg, "AltEnter");
  pressKey(dbg, "Enter");
  await waitForCondition(dbg, "1\n2");

  bp = findBreakpoint(dbg, "simple2", 5);
  is(bp.options.condition, "1\n2", "Hit 'Alt+Enter' adds a new line");

  clickElement(dbg, "gutter", 5);
  await waitForDispatch(dbg, "REMOVE_BREAKPOINT");
  bp = findBreakpoint(dbg, "simple2", 5);
  is(bp, null, "breakpoint was removed");
  await assertEditorBreakpoint(dbg, 5);

  info("Adding a condition to a breakpoint");
  clickElement(dbg, "gutter", 5);
  await waitForDispatch(dbg, "SET_BREAKPOINT");
  await setConditionalBreakpoint(dbg, 5, "1");
  await waitForCondition(dbg, 1);

  bp = findBreakpoint(dbg, "simple2", 5);
  is(bp.options.condition, "1", "breakpoint is created with the condition");
  await assertEditorBreakpoint(dbg, 5, { hasCondition: true });

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
  info('select "remove condition"');
  selectContextMenuItem(dbg, selectors.breakpointContextMenu.removeCondition);
  await waitForBreakpointWithoutCondition(dbg, "simple2", 5);
  bp = findBreakpoint(dbg, "simple2", 5);
  is(bp.options.condition, undefined, "breakpoint condition removed");

  info('Add "log point"');
  await setLogPoint(dbg, 5, "44");
  await waitForLog(dbg, 44);
  await assertEditorBreakpoint(dbg, 5, { hasLog: true });

  bp = findBreakpoint(dbg, "simple2", 5);
  is(bp.options.logValue, "44", "breakpoint condition removed");

  await altClickElement(dbg, "gutter", 6);
  bp = await waitForBreakpoint(dbg, "simple2", 6);
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

function getLineEl(dbg, line) {
  const lines = dbg.win.document.querySelectorAll(".CodeMirror-code > div");
  return lines[line - 1];
}

function assertEditorBreakpoint(
  dbg,
  line,
  { hasCondition = false, hasLog = false } = {}
) {
  const hasConditionClass = getLineEl(dbg, line).classList.contains(
    "has-condition"
  );

  ok(
    hasConditionClass === hasCondition,
    `Breakpoint condition ${
      hasCondition ? "exists" : "does not exist"
    } on line ${line}`
  );

  const hasLogClass = getLineEl(dbg, line).classList.contains("has-log");

  ok(
    hasLogClass === hasLog,
    `Breakpoint log ${hasLog ? "exists" : "does not exist"} on line ${line}`
  );
}

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
  selectContextMenuItem(dbg, selector);
  typeInPanel(dbg, condition);
}

async function setLogPoint(dbg, index, value) {
  rightClickElement(dbg, "gutter", index);
  selectContextMenuItem(
    dbg,
    `${selectors.addLogItem},${selectors.editLogItem}`
  );
  await typeInPanel(dbg, value);
}

async function waitForConditionalPanelFocus(dbg) {
  await waitFor(() => dbg.win.document.activeElement.tagName === "TEXTAREA");
}
