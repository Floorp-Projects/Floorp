/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function getColumnBreakpointElements(dbg) {
  return findAllElementsWithSelector(dbg, ".column-breakpoint");
}

async function assertConditionalBreakpointIsFocused(dbg) {
  const input = findElement(dbg, "conditionalPanelInput");
  await waitForElementFocus(dbg, input);
}

function waitForElementFocus(dbg, el) {
  const doc = dbg.win.document;
  return waitFor(() => doc.activeElement == el && doc.hasFocus());
}

function hasCondition(marker) {
  return marker.classList.contains("has-condition");
}

async function setConditionalBreakpoint(dbg, index, condition) {
  const {
      addConditionalBreakpoint,
      editBreakpoint
  } = selectors.gutterContextMenu;
  // Make this work with either add or edit menu items
  const selector = `${addConditionalBreakpoint},${editBreakpoint}`;

  rightClickElement(dbg, "breakpointItem", index);
  selectContextMenuItem(dbg, selector);
  await waitForElement(dbg, "conditionalPanelInput");
  await assertConditionalBreakpointIsFocused(dbg);

  // Position cursor reliably at the end of the text.
  pressKey(dbg, "End");
  type(dbg, condition);
  pressKey(dbg, "Enter");
}

function removeBreakpointViaContext(dbg, index) {
  rightClickElement(dbg, "breakpointItem", index);
  selectContextMenuItem(dbg, "#node-menu-delete-self");
}

// Test enabling and disabling a breakpoint using the check boxes
add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple1");
  await pushPref("devtools.debugger.features.column-breakpoints", true);

  if(!Services.prefs.getBoolPref("devtools.debugger.features.column-breakpoints")) {
    ok(true, "This test only applies when column breakpoints are on");
    return;
  }

  await selectSource(dbg, "simple1");

  // Scroll down to desired line so that column breakpoints render
  getCM(dbg).setCursor({ line: 15, ch: 0 });

  // Create a breakpoint at 15:undefined
  await addBreakpoint(dbg, "simple1", 15);
  
  // Wait for column breakpoint markers
  await waitForElementWithSelector(dbg, ".column-breakpoint");

  let columnBreakpointMarkers = getColumnBreakpointElements(dbg);
  ok(
    columnBreakpointMarkers.length === 2, 
      "2 column breakpoint markers display"
  );

  // Create a breakpoint at 15:8
  columnBreakpointMarkers[0].click();

  // Create a breakpoint at 15:28
  columnBreakpointMarkers[1].click();

  // Wait for breakpoints in right panel to render
  await waitForState(dbg, state => {
    return dbg.win.document.querySelectorAll(".breakpoints-list .breakpoint").length === 3;
  })

  // Scroll down in secondary pane so element we want to right-click is showing
  dbg.win.document.querySelector(".secondary-panes").scrollTop = 100;

  // Set a condition at 15:8
  await setConditionalBreakpoint(dbg, 4, "Eight");

  // Ensure column breakpoint is yellow
  await waitForElementWithSelector(dbg, ".column-breakpoint.has-condition");
  
  // Remove the breakpoint from 15:undefined via the secondary pane context menu
  removeBreakpointViaContext(dbg, 3);

  // Ensure that there's still a marker on line 15
  await waitForState(dbg, state => dbg.selectors.getBreakpointCount(state) == 2);
  await waitForElementWithSelector(dbg, ".column-breakpoint.has-condition");
  columnBreakpointMarkers = getColumnBreakpointElements(dbg);
  ok(hasCondition(columnBreakpointMarkers[0]), "First column breakpoint has conditional style");

  // Remove the breakpoint from 15:8
  removeBreakpointViaContext(dbg, 3);

  // Ensure there's still a marker and it has no condition
  await waitForState(dbg, state => dbg.selectors.getBreakpointCount(state) == 1);
  await waitForElementWithSelector(dbg, ".column-breakpoint");

  // Ensure the first column breakpoint has no conditional style
  await waitFor(() => !hasCondition(getColumnBreakpointElements(dbg)[0]));
});
