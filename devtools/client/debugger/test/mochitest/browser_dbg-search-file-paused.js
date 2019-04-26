/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests the search bar correctly responds to queries, enter, shift enter

function waitForSearchState(dbg) {
  return waitForState(dbg, () => getCM(dbg).state.search);
}

function getFocusedEl(dbg) {
  const doc = dbg.win.document;
  return doc.activeElement;
}

function pressMouseDown(dbg, node) {
  EventUtils.sendMouseEvent({ type: "mousedown" }, node, dbg.win);
}

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple1.js", "simple2.js");
  const {
    selectors: { getBreakpoints, getBreakpoint, getActiveSearch },
    getState
  } = dbg;

  info("Add a breakpoint, wait for pause");
  const source = findSource(dbg, "simple2.js");
  await selectSource(dbg, source.url);
  await addBreakpoint(dbg, source, 5);
  invokeInTab("main");
  await waitForPaused(dbg);

  info("Starting a search for 'bar'");
  const cm = getCM(dbg);
  pressKey(dbg, "fileSearch");
  is(dbg.selectors.getActiveSearch(), "file");
  const el = getFocusedEl(dbg);
  type(dbg, "bar");
  await waitForSearchState(dbg);

  info("Ensuring 'bar' matches are highlighted");
  pressKey(dbg, "Enter");
  is(cm.state.search.posFrom.line, 1);
  pressKey(dbg, "Enter");
  is(cm.state.search.posFrom.line, 4);

  info("Switching files via frame click");
  const frames = findAllElements(dbg, "frames");
  pressMouseDown(dbg, frames[1])

  // Ensure that the debug line is in view, and not the first "bar" instance,
  // which the user would have to scroll down for
  const { top } = cm.getScrollInfo();
  is(top, 0, "First search term is not in view");

  // Change the search term and go back to the first source in stack
  info("Switching to paused file via frame click");
  pressKey(dbg, "fileSearch");
  el.value = "";
  type(dbg, "func");
  await waitForSearchState(dbg);
  pressMouseDown(dbg, frames[0]);
  await waitFor(() => cm.state.search.query === "func");

  // Ensure there is a match for the new term
  pressKey(dbg, "Enter");
  is(cm.state.search.posFrom.line, 0);
  pressKey(dbg, "Enter");
  is(cm.state.search.posFrom.line, 1);
});
