/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * This test focuses on the SourceTree component, within the browser toolbox.
 */

"use strict";

requestLongerTimeout(2);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/browser-toolbox/test/helpers-browser-toolbox.js",
  this
);

// Test that the Web extension name is shown in source tree rather than
// the extensions internal UUID. This checks both the web toolbox and the
// browser toolbox.
add_task(async function testSourceTreeNamesForWebExtensions() {
  await pushPref("devtools.chrome.enabled", true);
  await pushPref("devtools.browsertoolbox.scope", "everything");
  const extension = await installAndStartContentScriptExtension();

  const dbg = await initDebugger("doc-content-script-sources.html");
  await waitForSourcesInSourceTree(dbg, [], {
    noExpand: true,
  });

  is(
    getSourceTreeLabel(dbg, 2),
    "Test content script extension",
    "Test content script extension is labeled properly"
  );

  await dbg.toolbox.closeToolbox();
  await extension.unload();

  // Make sure the toolbox opens with the debugger selected.
  await pushPref("devtools.browsertoolbox.panel", "jsdebugger");

  const ToolboxTask = await initBrowserToolboxTask();
  await ToolboxTask.importFunctions({
    createDebuggerContext,
    waitUntil,
    findSourceNodeWithText,
    findAllElements,
    getSelector,
    findAllElementsWithSelector,
    assertSourceTreeNode,
  });

  await ToolboxTask.spawn(selectors, async _selectors => {
    this.selectors = _selectors;
  });

  await ToolboxTask.spawn(null, async () => {
    try {
      /* global gToolbox */
      // Wait for the debugger to finish loading.
      await gToolbox.getPanelWhenReady("jsdebugger");
      const dbgx = createDebuggerContext(gToolbox);
      let rootNodeForExtensions = null;
      await waitUntil(() => {
        rootNodeForExtensions = findSourceNodeWithText(dbgx, "extension");
        return !!rootNodeForExtensions;
      });
      // Find the root node for extensions and expand it if needed
      if (
        !!rootNodeForExtensions &&
        !rootNodeForExtensions.querySelector(".arrow.expanded")
      ) {
        rootNodeForExtensions.querySelector(".arrow").click();
      }

      // Assert that extensions are displayed in the source tree
      // with their extension name.
      await assertSourceTreeNode(dbgx, "Picture-In-Picture");
      await assertSourceTreeNode(dbgx, "Form Autofill");

      const threadLabels = [...findAllElements(dbgx, "sourceTreeThreads")].map(
        el => {
          return el.textContent;
        }
      );
      is(
        threadLabels[0],
        "Main Thread",
        "The first thread is always the main thread"
      );
      let lastPID = -1,
        lastThreadLabel = "";
      for (let i = 1; i < threadLabels.length; i++) {
        const label = threadLabels[i];
        if (label.startsWith("(pid ")) {
          ok(
            !lastThreadLabel,
            "We should only have content process threads first after the main thread"
          );
          const pid = parseInt(label.match(/pid (\d+)\)/)[1], 10);
          ok(
            pid >= lastPID,
            `The content process threads are sorted by incremental PID ${pid} > ${lastPID}`
          );
          lastPID = pid;
        } else {
          ok(
            label.localeCompare(lastThreadLabel) >= 0,
            `Worker thread labels are sorted alphabeticaly: ${label} vs ${lastThreadLabel}`
          );
          lastThreadLabel = label;
        }
      }
    } catch (e) {
      console.log("Caught exception in spawn", e);
      throw e;
    }
  });

  await ToolboxTask.destroy();
});
