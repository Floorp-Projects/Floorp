/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test enabling and disabling a breakpoint using the check boxes

"use strict";
// Test enabling/disabling the breakpoints via the checkboxes
add_task(async function () {
  const dbg = await initDebugger("doc-scripts.html", "simple2.js");

  const source = findSource(dbg, "simple2.js");
  await selectSource(dbg, source);

  await addBreakpointViaGutter(dbg, 5);

  is(
    findBreakpoint(dbg, "simple2.js", 5).disabled,
    false,
    "Breakpoint on line 5 is enabled"
  );

  info("Disable the breakpoint");
  await disableBreakpoint(dbg, 0);

  is(
    findBreakpoint(dbg, "simple2.js", 5).disabled,
    true,
    "first breakpoint is disabled"
  );

  invokeInTab("simple");
  await wait(1000);
  assertNotPaused(dbg, "The disabled breakpoint is not hit");

  info("Re-enable the breakpoint on line 5");
  await enableBreakpoint(dbg, 0);

  is(
    findBreakpoint(dbg, "simple2.js", 5).disabled,
    false,
    "Breakpoint on line 5 is enabled"
  );

  invokeInTab("simple");
  await waitForPaused(dbg);

  assertPausedAtSourceAndLine(dbg, source.id, 5);
  await resume(dbg);

  await removeBreakpointViaGutter(dbg, 5);
});

// Test enabling and disabling a breakpoint using the context menu
add_task(async function () {
  const dbg = await initDebugger("doc-scripts.html", "simple2.js");

  const source = findSource(dbg, "simple2.js");
  await selectSource(dbg, source);

  await addBreakpointViaGutter(dbg, 5);
  assertBreakpointSnippet(dbg, 5, "return bar;");

  info("Disable the breakpoint on line 5 using the breakpoint context menu");
  await triggerBreakpointItemContextMenu(
    dbg,
    2,
    selectors.breakpointContextMenu.disableSelf
  );

  is(
    findBreakpoint(dbg, "simple2.js", 5).disabled,
    true,
    "Breakpoint on line 5 is enabled"
  );

  invokeInTab("simple");
  await wait(1000);
  assertNotPaused(dbg, "The disabled breakpoint is not hit");

  info("Enable the breakpoint on line 5 using the breakpoint context menu");
  await triggerBreakpointItemContextMenu(
    dbg,
    2,
    selectors.breakpointContextMenu.enableSelf
  );

  is(
    findBreakpoint(dbg, "simple2.js", 5).disabled,
    false,
    "first breakpoint is enabled"
  );

  invokeInTab("simple");
  await waitForPaused(dbg);

  assertPausedAtSourceAndLine(dbg, source.id, 5);
  await resume(dbg);

  await removeBreakpointViaGutter(dbg, 5);
});

// Tests creation of disabled breakpoint with shift-click
// and that disabled breakpoints are not triggered across toolbox restarts
add_task(async function testDisabledBreakpointsViaKeyShortcut() {
  const dbg = await initDebugger("doc-scripts.html", "simple2.js");

  const source = findSource(dbg, "simple2.js");
  await selectSource(dbg, source);

  await shiftClickElement(dbg, "gutter", 5);
  await waitForBreakpoint(dbg, "simple2.js", 5);

  is(
    findBreakpoint(dbg, "simple2.js", 5).disabled,
    true,
    "breakpoint is disabled"
  );

  invokeInTab("simple");
  info("Wait a bit to make sure there is no pause");
  await wait(1000);
  assertNotPaused(dbg, "The disabled breakpoint is not hit");

  info("Close and reopen the web toolbox");
  await dbg.toolbox.closeToolbox();
  const toolbox = await openToolboxForTab(gBrowser.selectedTab, "webconsole");

  invokeInTab("simple");
  await wait(1000);

  info(
    "Assert that the there was no pause, and there was no switch to the debugger"
  );
  const panel = toolbox.getPanel("jsdebugger");
  ok(!panel, "There was no pause and switch to the debugger ");

  info("Switch to the debugger and cleanup the breakpoint");
  await toolbox.selectTool("jsdebugger");
  const dbg2 = createDebuggerContext(toolbox);
  assertNotPaused(dbg);
  await removeBreakpointViaGutter(dbg2, 5);
});

function toggleBreakpoint(dbg, index) {
  const bp = findAllElements(dbg, "breakpointItems")[index];
  const input = bp.querySelector("input");
  input.click();
}

async function disableBreakpoint(dbg, index) {
  const disabled = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  toggleBreakpoint(dbg, index);
  await disabled;
}

async function enableBreakpoint(dbg, index) {
  const enabled = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  toggleBreakpoint(dbg, index);
  await enabled;
}

async function triggerBreakpointItemContextMenu(dbg, index, contextMenuItem) {
  rightClickElement(dbg, "breakpointItem", index);
  await waitForContextMenu(dbg);
  selectContextMenuItem(dbg, contextMenuItem);
  return waitForDispatch(dbg.store, "SET_BREAKPOINT");
}
