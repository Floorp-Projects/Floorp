/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests breakpoints are stored when opening the toolbox or the debugger

"use strict";

// Test without source maps
add_task(async function () {
  let dbg = await initDebugger("doc-scripts.html", "simple3.js");
  await selectSource(dbg, "simple3.js");
  // First register a valid breakpoint
  await addBreakpoint(dbg, "simple3.js", 3);
  // Also register a disabled breakpoint just before, to ensure it is really kept disabled
  await addBreakpoint(dbg, "simple3.js", 2, 15);
  await disableBreakpoint(dbg, findSource(dbg, "simple3.js"), 2, 15);

  // Pause to verify the breakpoints are correct
  invokeInTab("simple");

  await waitForPaused(dbg);
  await assertPausedAtSourceAndLine(dbg, findSource(dbg, "simple3.js").id, 3);
  await resume(dbg);

  // Now close the toolbox
  await dbg.toolbox.closeToolbox();

  // Re-open the toolbox, but on the console
  const toolbox = await openToolboxForTab(gBrowser.selectedTab, "webconsole");

  // Re-pause
  const onSelected = toolbox.once("jsdebugger-selected");
  invokeInTab("simple");
  await onSelected;

  // Verify the breakpoints are correctly restored
  dbg = createDebuggerContext(toolbox);
  await waitForPaused(dbg);
  await assertPausedAtSourceAndLine(dbg, findSource(dbg, "simple3.js").id, 3);
  await resume(dbg);

  is(
    dbg.selectors.getBreakpointCount(),
    2,
    "The two breakpoints are preserved"
  );
});

// Test with source maps and original sources
add_task(async function () {
  let dbg = await initDebugger("doc-sourcemaps.html", "times2.js");
  await selectSource(dbg, "times2.js");
  // First registry a simple breakpoint
  await addBreakpoint(dbg, "times2.js", 2);

  // Pause to verify the breakpoint works
  invokeInTab("keepMeAlive");

  await waitForPaused(dbg, null, {
    shouldWaitForLoadedScopes: false,
  });
  let source = findSource(dbg, "times2.js");
  await assertPausedAtSourceAndLine(dbg, source.id, 2);
  await resume(dbg);

  // Now close the toolbox
  await dbg.toolbox.closeToolbox();

  // Re-open the toolbox, but on the console
  const toolbox = await openToolboxForTab(gBrowser.selectedTab, "webconsole");

  // Re-pause
  const onSelected = toolbox.once("jsdebugger-selected");
  invokeInTab("keepMeAlive");
  await onSelected;

  // Verify the breakpoints are correctly restored
  dbg = createDebuggerContext(toolbox);
  await waitForPaused(dbg, null, {
    shouldWaitForLoadedScopes: false,
  });
  source = findSource(dbg, "times2.js");
  await assertPausedAtSourceAndLine(dbg, source.id, 2);
  await resume(dbg);

  is(dbg.selectors.getBreakpointCount(), 1, "The breakpoint is preserved");
});
