/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that all kinds of workers show up properly in the multiprocess browser
// toolbox.

"use strict";

requestLongerTimeout(4);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/browser-toolbox/test/helpers-browser-toolbox.js",
  this
);

add_task(async function() {
  await pushPref("devtools.browsertoolbox.fission", true);
  await pushPref("dom.serviceWorkers.enabled", true);
  await pushPref("dom.serviceWorkers.testing.enabled", true);

  const ToolboxTask = await initBrowserToolboxTask();
  await ToolboxTask.importFunctions({ waitUntil, waitForAllTargetsToBeAttached });

  await addTab(EXAMPLE_URL + "doc-all-workers.html");

  await ToolboxTask.spawn(null, async () => {
    await gToolbox.selectTool("jsdebugger");
    const dbg = gToolbox.getCurrentPanel().panelWin.dbg;

    await waitUntil(() => {
      const threads = dbg.selectors.getThreads();
      return (
        hasWorker("simple-worker.js") &&
        hasWorker("shared-worker.js") &&
        hasWorker("service-worker.sjs")
      );

      function hasWorker(workerName) {
        return threads.some(({ name }) => name == workerName);
      }
    });

    await waitForAllTargetsToBeAttached(gToolbox.commands.targetCommand);
  });
  ok(true, "All workers appear in browser toolbox debugger");

  invokeInTab("unregisterServiceWorker");

  await ToolboxTask.destroy();
});
