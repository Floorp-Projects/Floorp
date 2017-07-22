/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function findBreakpoint(dbg, url, line) {
  const { selectors: { getBreakpoint }, getState } = dbg;
  const source = findSource(dbg, url);
  return getBreakpoint(getState(), { sourceId: source.id, line });
}

function setConditionalBreakpoint(dbg, index, condition) {
  return Task.spawn(function*() {
    rightClickElement(dbg, "gutter", index);
    selectMenuItem(dbg, 2);
    yield waitForElement(dbg, ".conditional-breakpoint-panel input");
    findElementWithSelector(dbg, ".conditional-breakpoint-panel input").focus();
    // Position cursor reliably at the end of the text.
    pressKey(dbg, "End");
    type(dbg, condition);
    pressKey(dbg, "Enter");
  });
}

add_task(function*() {
  const dbg = yield initDebugger("doc-scripts.html");
  yield selectSource(dbg, "simple2");

  // Adding a conditional Breakpoint
  yield setConditionalBreakpoint(dbg, 5, "1");
  yield waitForDispatch(dbg, "ADD_BREAKPOINT");
  let bp = findBreakpoint(dbg, "simple2", 5);
  is(bp.condition, "1", "breakpoint is created with the condition");

  // Editing a conditional Breakpoint
  yield setConditionalBreakpoint(dbg, 5, "2");
  yield waitForDispatch(dbg, "SET_BREAKPOINT_CONDITION");
  bp = findBreakpoint(dbg, "simple2", 5);
  is(bp.condition, "12", "breakpoint is created with the condition");

  // Removing a conditional breakpoint
  clickElement(dbg, "gutter", 5);
  yield waitForDispatch(dbg, "REMOVE_BREAKPOINT");
  bp = findBreakpoint(dbg, "simple2", 5);
  is(bp, null, "breakpoint was removed");

  // Adding a condition to a breakpoint
  clickElement(dbg, "gutter", 5);
  yield waitForDispatch(dbg, "ADD_BREAKPOINT");
  yield setConditionalBreakpoint(dbg, 5, "1");
  yield waitForDispatch(dbg, "SET_BREAKPOINT_CONDITION");

  bp = findBreakpoint(dbg, "simple2", 5);
  is(bp.condition, "1", "breakpoint is created with the condition");
});
