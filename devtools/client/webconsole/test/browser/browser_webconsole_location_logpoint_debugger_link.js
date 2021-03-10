/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test clicking locations of logpoint logs and errors will open corresponding
// conditional panels in the debugger.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-location-debugger-link-logpoint.html";

add_task(async function() {
  // On e10s, the exception thrown in test-location-debugger-link-errors.js
  // is triggered in child process and is ignored by test harness
  if (!Services.appinfo.browserTabsRemoteAutostart) {
    expectUncaughtException();
  }

  // Eliminate interference from "saved" breakpoints
  // when running the test multiple times
  await clearDebuggerPreferences();
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Open the Debugger panel");
  await openDebugger();

  const toolbox = hud.toolbox;
  const dbg = createDebuggerContext(toolbox);
  await selectSource(dbg, "test-location-debugger-link-logpoint-1.js");

  info("Add a logpoint with an invalid expression");
  await setLogPoint(dbg, 7, "undefinedVariable");

  info("Add a logpoint with a valid expression");
  await setLogPoint(dbg, 8, "`a is ${a}`");

  await assertEditorLogpoint(dbg, 7, { hasLog: true });
  await assertEditorLogpoint(dbg, 8, { hasLog: true });

  info("Close the file in the debugger");
  await closeTab(dbg, "test-location-debugger-link-logpoint-1.js");

  info("Selecting the console");
  await toolbox.selectTool("webconsole");

  info("Call the function");
  await invokeInTab("add");

  info("Wait for two messages");
  await waitFor(() => findMessages(hud, "").length === 2);

  await testOpenInDebugger(
    hud,
    toolbox,
    "undefinedVariable is not defined",
    true,
    false,
    false,
    "undefinedVariable"
  );

  info("Selecting the console again");
  await toolbox.selectTool("webconsole");
  await testOpenInDebugger(
    hud,
    toolbox,
    "a is 1",
    true,
    false,
    false,
    "`a is ${a}`"
  );

  // Test clicking location of a removed logpoint, or a newly added breakpoint
  // at an old logpoint's location will only highlight its line
  info("Remove the logpoints");
  const source = await findSource(
    dbg,
    "test-location-debugger-link-logpoint-1.js"
  );
  await removeBreakpoint(dbg, source.id, 7);
  await removeBreakpoint(dbg, source.id, 8);
  await addBreakpoint(dbg, "test-location-debugger-link-logpoint-1.js", 8);

  info("Selecting the console");
  await toolbox.selectTool("webconsole");
  await testOpenInDebugger(
    hud,
    toolbox,
    "undefinedVariable is not defined",
    true,
    8,
    10
  );

  info("Selecting the console again");
  await toolbox.selectTool("webconsole");
  await testOpenInDebugger(hud, toolbox, "a is 1", true, 8, 10);
});

// Test clicking locations of logpoints from different files
add_task(async function() {
  if (!Services.appinfo.browserTabsRemoteAutostart) {
    expectUncaughtException();
  }

  await clearDebuggerPreferences();
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Open the Debugger panel");
  await openDebugger();

  const toolbox = hud.toolbox;
  const dbg = createDebuggerContext(toolbox);

  info("Add a logpoint to the first file");
  await selectSource(dbg, "test-location-debugger-link-logpoint-1.js");
  await setLogPoint(dbg, 8, "`a is ${a}`");

  info("Add a logpoint to the second file");
  await selectSource(dbg, "test-location-debugger-link-logpoint-2.js");
  await setLogPoint(dbg, 8, "`c is ${c}`");

  info("Selecting the console");
  await toolbox.selectTool("webconsole");

  info("Call the function from the first file");
  await invokeInTab("add");

  info("Wait for the first message");
  await waitFor(() => findMessages(hud, "").length === 1);
  await testOpenInDebugger(
    hud,
    toolbox,
    "a is 1",
    true,
    false,
    false,
    "`a is ${a}`"
  );

  info("Selecting the console again");
  await toolbox.selectTool("webconsole");

  info("Call the function from the second file");
  await invokeInTab("subtract");

  info("Wait for the second message");
  await waitFor(() => findMessages(hud, "").length === 2);
  await testOpenInDebugger(
    hud,
    toolbox,
    "c is 1",
    true,
    false,
    false,
    "`c is ${c}`"
  );
});

async function setLogPoint(dbg, index, expression) {
  rightClickElement(dbg, "gutter", index);
  selectContextMenuItem(
    dbg,
    `${selectors.addLogItem},${selectors.editLogItem}`
  );
  const onBreakpointSet = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  await typeInPanel(dbg, expression);
  await onBreakpointSet;
}

function getLineEl(dbg, line) {
  const lines = dbg.win.document.querySelectorAll(".CodeMirror-code > div");
  return lines[line - 1];
}

function assertEditorLogpoint(dbg, line, { hasLog = false } = {}) {
  const hasLogClass = getLineEl(dbg, line).classList.contains("has-log");

  ok(
    hasLogClass === hasLog,
    `Breakpoint log ${hasLog ? "exists" : "does not exist"} on line ${line}`
  );
}
