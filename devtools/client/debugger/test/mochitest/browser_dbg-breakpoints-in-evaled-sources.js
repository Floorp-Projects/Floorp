/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// this evaled source includes a "debugger" statement so that it gets
// automatically opened in the debugger when it is executed.
// We wrap it in a setTimeout to avoid errors in the webconsole actor which
// would still be processing the outcome of the evaluation after we destroy
// the thread actor.

"use strict";

const EVALED_SOURCE_TEXT = `setTimeout(function() {
  debugger;
  console.log("SECOND LINE");
}, 10)`;

/**
 * Check against blank debugger panel issues when attempting to restore
 * breakpoints set in evaled sources (Bug 1720512).
 *
 * The STRs triggering this bug require to:
 * - set a valid breakpoint on a regular source
 * - then set a breakpoint on an evaled source
 * - close and reopen the debugger
 *
 * This test will follow those STRs while also performing a few additional
 * checks (eg verify breakpoints can be hit at various stages of the test).
 */
add_task(async function () {
  info("Open the debugger and set a breakpoint on a regular script");
  const dbg = await initDebugger("doc-scripts.html");
  await selectSource(dbg, "doc-scripts.html");
  await addBreakpoint(dbg, "doc-scripts.html", 21);

  info("Switch to the console and evaluate a source with a debugger statement");
  const { hud } = await dbg.toolbox.selectTool("webconsole");
  const onSelected = dbg.toolbox.once("jsdebugger-selected");
  await hud.ui.wrapper.dispatchEvaluateExpression(EVALED_SOURCE_TEXT);

  info("Wait for the debugger to be selected");
  await onSelected;

  info("Wait for the debugger to be paused on the debugger statement");
  await waitForPaused(dbg);

  is(
    getCM(dbg).getValue(),
    EVALED_SOURCE_TEXT,
    "The debugger is showing the evaled source"
  );

  const evaledSource = dbg.selectors.getSelectedSource();
  assertPausedAtSourceAndLine(dbg, evaledSource.id, 2);

  info("Add a breakpoint in the evaled source");
  await addBreakpoint(dbg, evaledSource, 3);

  info("Resume and check that we hit the breakpoint");
  await resume(dbg);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, evaledSource.id, 3);

  info("Close the toolbox");
  await dbg.toolbox.closeToolbox();

  info("Reopen the toolbox on the debugger");
  const toolbox2 = await openToolboxForTab(gBrowser.selectedTab, "jsdebugger");
  const dbg2 = createDebuggerContext(toolbox2);

  // The initial regression tested here led to a blank debugger,
  // if we can see the doc-scripts.html source, this already means the debugger
  // is functional.
  await waitForSources(dbg2, "doc-scripts.html");

  // Wait until codeMirror is rendered before reloading the debugger.
  await waitFor(() => findElement(dbg2, "codeMirror"));

  info("Reload to check if we hit the breakpoint added in doc-scripts.html");
  const onReloaded = reload(dbg2);

  await waitForDispatch(dbg2.store, "NAVIGATE");
  await waitForSelectedSource(dbg2, "doc-scripts.html");
  await waitForPaused(dbg2);

  const scriptSource = dbg2.selectors.getSelectedSource();
  assertPausedAtSourceAndLine(dbg2, scriptSource.id, 21);
  await resume(dbg2);

  info("Wait for reload to complete after resume");
  await onReloaded;
});
