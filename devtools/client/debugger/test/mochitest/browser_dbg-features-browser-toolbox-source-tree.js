/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * This test focuses on the SourceTree component, within the browser toolbox.
 */

"use strict";

requestLongerTimeout(2);

/* import-globals-from ../../../framework/browser-toolbox/test/helpers-browser-toolbox.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/browser-toolbox/test/helpers-browser-toolbox.js",
  this
);

// Test that the Web extension name is shown in source tree rather than
// the extensions internal UUID. This checks both the web toolbox and the
// browser toolbox.
add_task(async function testSourceTreeNamesForWebExtensions() {
  await pushPref("devtools.chrome.enabled", true);
  await pushPref("devtools.browsertoolbox.fission", true);
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
    } catch (e) {
      console.log("Caught exception in spawn", e);
      throw e;
    }
  });

  await ToolboxTask.destroy();
});
