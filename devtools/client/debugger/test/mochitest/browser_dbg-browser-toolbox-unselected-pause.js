/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that the debugger pauses in the multiprocess browser toolbox even when
// it hasn't been opened.

"use strict";

requestLongerTimeout(4);

/* import-globals-from ../../../framework/browser-toolbox/test/helpers-browser-toolbox.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/browser-toolbox/test/helpers-browser-toolbox.js",
  this
);

add_task(async function() {
  await pushPref("devtools.browsertoolbox.fission", true);

  // Make sure the toolbox opens with the webconsole initially selected.
  await pushPref("devtools.browsertoolbox.panel", "webconsole");

  const ToolboxTask = await initBrowserToolboxTask();
  await ToolboxTask.importFunctions({
    createDebuggerContext,
    waitUntil,
    waitForPaused,
    isPaused,
    waitForState,
    waitForSelectedSource,
    waitForLoadedScopes,
    waitForElement,
    findElement,
    getSelector,
    findElementWithSelector,
  });
  // ToolboxTask.spawn pass input arguments by stringify them via string concatenation.
  // This mean we have to stringify the input object, but don't have to parse it from the task.
  await ToolboxTask.spawn(selectors, async _selectors => {
    this.selectors = _selectors;
  });

  addTab("data:text/html,<script>debugger;</script>");

  // The debugger should automatically be selected.
  await ToolboxTask.spawn(null, async () => {
    /* global gToolbox */
    await waitUntil(() => gToolbox.currentToolId == "jsdebugger");
  });
  ok(true, "Debugger selected");

  // The debugger should pause.
  await ToolboxTask.spawn(null, async () => {
    // Wait for the debugger to finish loading.
    await gToolbox.getPanelWhenReady("jsdebugger");

    const dbg = createDebuggerContext(gToolbox);
    await waitForPaused(dbg);
    if (!gToolbox.isHighlighted("jsdebugger")) {
      throw new Error("Debugger not highlighted");
    }
  });
  ok(true, "Paused in new tab");

  await ToolboxTask.destroy();
});
