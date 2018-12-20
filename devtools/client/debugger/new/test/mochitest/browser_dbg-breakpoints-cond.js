/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

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
    "Breakpoint " +
      (shouldExist ? "exists" : "does not exist") +
      " on line " +
      line
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
    editBreakpoint
  } = selectors.gutterContextMenu;
  // Make this work with either add or edit menu items
  const selector = `${addConditionalBreakpoint},${editBreakpoint}`;

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
  await selectSource(dbg, "simple2");
  await waitForSelectedSource(dbg, "simple2");

  await setConditionalBreakpoint(dbg, 5, "1");
  await waitForDispatch(dbg, "ADD_BREAKPOINT");
  
  let bp = findBreakpoint(dbg, "simple2", 5);
  is(bp.condition, "1", "breakpoint is created with the condition");
  assertEditorBreakpoint(dbg, 5, true);

  await setConditionalBreakpoint(dbg, 5, "2");
  await waitForDispatch(dbg, "SET_BREAKPOINT_CONDITION");
  bp = findBreakpoint(dbg, "simple2", 5);
  is(bp.condition, "2", "breakpoint is created with the condition");
  assertEditorBreakpoint(dbg, 5, true);

  clickElement(dbg, "gutter", 5);
  await waitForDispatch(dbg, "REMOVE_BREAKPOINT");
  bp = findBreakpoint(dbg, "simple2", 5);
  is(bp, null, "breakpoint was removed");
  assertEditorBreakpoint(dbg, 5, false);

  // Adding a condition to a breakpoint
  clickElement(dbg, "gutter", 5);
  await waitForDispatch(dbg, "ADD_BREAKPOINT");
  await setConditionalBreakpoint(dbg, 5, "1");
  await waitForDispatch(dbg, "SET_BREAKPOINT_CONDITION");

  bp = findBreakpoint(dbg, "simple2", 5);
  is(bp.condition, "1", "breakpoint is created with the condition");
  assertEditorBreakpoint(dbg, 5, true);

  const bpCondition = waitForDispatch(dbg, "SET_BREAKPOINT_CONDITION");
  //right click breakpoint in breakpoints list
  rightClickElement(dbg, "breakpointItem", 3)
  // select "remove condition";
  selectContextMenuItem(dbg, selectors.breakpointContextMenu.removeCondition);
  await bpCondition;
  bp = findBreakpoint(dbg, "simple2", 5);
  is(bp.condition, undefined, "breakpoint condition removed");
});
