/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

function findBreakpoint(dbg, url, line, column = 0) {
  const {
    selectors: { getBreakpoint },
    getState
  } = dbg;
  const source = findSource(dbg, url);
  const location = { sourceId: source.id, line, column };
  return getBreakpoint(getState(), location);
}

function getLineEl(dbg, line) {
  const lines = dbg.win.document.querySelectorAll(".CodeMirror-code > div");
  return lines[line - 1];
}

function assertEditorBreakpoint(dbg, line, shouldExist) {
  const exists = getLineEl(dbg, line).classList.contains("has-condition");

  ok(
    exists === shouldExist,
    `Breakpoint ${shouldExist ? "exists" : "does not exist"} on line ${line}`
  );
}

function waitForElementFocus(dbg, el) {
  const doc = dbg.win.document;
  return waitFor(() => doc.activeElement == el && doc.hasFocus());
}

async function assertConditionalBreakpointIsFocused(dbg) {
  const input = findElement(dbg, "conditionalPanelInput");
  await waitForElementFocus(dbg, input);
}

async function setConditionalBreakpoint(dbg, index, condition) {
  const {
    addConditionalBreakpoint,
    editConditionalBreakpoint
  } = selectors.gutterContextMenu;
  // Make this work with either add or edit menu items
  const selector = `${addConditionalBreakpoint},${editConditionalBreakpoint}`;

  rightClickElement(dbg, "gutter", index);
  selectContextMenuItem(dbg, selector);
  await waitForElement(dbg, "conditionalPanelInput");
  await assertConditionalBreakpointIsFocused(dbg);

  // Position cursor reliably at the end of the text.
  pressKey(dbg, "End");
  type(dbg, condition);
  pressKey(dbg, "Enter");
}

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple2");
  await pushPref("devtools.debugger.features.column-breakpoints", false);

  await selectSource(dbg, "simple2");
  await waitForSelectedSource(dbg, "simple2");

  await setConditionalBreakpoint(dbg, 5, "1");
  await waitForDispatch(dbg, "ADD_BREAKPOINT");

  let bp = findBreakpoint(dbg, "simple2", 5);
  is(bp.options.condition, "1", "breakpoint is created with the condition");
  assertEditorBreakpoint(dbg, 5, true);

  // Edit the conditional breakpoint set above
  const bpCondition1 = waitForDispatch(dbg, "SET_BREAKPOINT_OPTIONS");
  await setConditionalBreakpoint(dbg, 5, "2");
  await bpCondition1;

  bp = findBreakpoint(dbg, "simple2", 5);
  is(bp.options.condition, "12", "breakpoint is created with the condition");
  assertEditorBreakpoint(dbg, 5, true);

  clickElement(dbg, "gutter", 5);
  await waitForDispatch(dbg, "REMOVE_BREAKPOINT");
  bp = findBreakpoint(dbg, "simple2", 5);
  is(bp, null, "breakpoint was removed");
  assertEditorBreakpoint(dbg, 5, false);

  // Adding a condition to a breakpoint
  clickElement(dbg, "gutter", 5);
  await waitForDispatch(dbg, "ADD_BREAKPOINT");
  const bpCondition2 = waitForDispatch(dbg, "SET_BREAKPOINT_OPTIONS");
  await setConditionalBreakpoint(dbg, 5, "1");
  await bpCondition2;

  bp = findBreakpoint(dbg, "simple2", 5);
  is(bp.options.condition, "1", "breakpoint is created with the condition");
  assertEditorBreakpoint(dbg, 5, true);

  const bpCondition3 = waitForDispatch(dbg, "SET_BREAKPOINT_OPTIONS");
  // right click breakpoint in breakpoints list
  rightClickElement(dbg, "breakpointItem", 3);
  // select "remove condition";
  selectContextMenuItem(dbg, selectors.breakpointContextMenu.removeCondition);
  await bpCondition3;
  bp = findBreakpoint(dbg, "simple2", 5);
  is(bp.options.condition, undefined, "breakpoint condition removed");
});
