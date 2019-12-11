/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that inspecting an optimized out variable works when execution is
// paused.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/" +
  "test-closure-optimized-out.html";

add_task(async function() {
  const breakpointLine = 18;
  const hud = await openNewTabAndConsole(TEST_URI);
  await openDebugger();

  const toolbox = hud.toolbox;
  const dbg = createDebuggerContext(toolbox);

  await selectSource(dbg, "test-closure-optimized-out.html");
  await addBreakpoint(dbg, "test-closure-optimized-out.html", breakpointLine);

  // Cause the debuggee to pause
  await pauseDebugger(dbg);

  await toolbox.selectTool("webconsole");

  // This is the meat of the test: evaluate the optimized out variable.
  info("Waiting for optimized out message");
  await executeAndWaitForMessage(hud, "upvar", "optimized out", ".result");
  ok(true, "Optimized out message logged");

  info("Open the debugger");
  await openDebugger();

  info("Resume");
  await resume(dbg);

  info("Remove the breakpoint");
  const source = findSource(dbg, "test-closure-optimized-out.html");
  await removeBreakpoint(dbg, source.id, breakpointLine);
});

async function pauseDebugger(dbg) {
  info("Waiting for debugger to pause");
  const onPaused = waitForPaused(dbg);
  ContentTask.spawn(gBrowser.selectedBrowser, {}, async function() {
    const button = content.document.querySelector("button");
    button.click();
  });
  await onPaused;
}
