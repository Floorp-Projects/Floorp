/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that all kinds of workers show up properly in the multiprocess browser
// toolbox.

"use strict";

requestLongerTimeout(4);

/* import-globals-from ../../../framework/browser-toolbox/test/helpers-browser-toolbox.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/browser-toolbox/test/helpers-browser-toolbox.js",
  this
);

add_task(async function() {
  await pushPref("devtools.browsertoolbox.fission", true);
  await pushPref("devtools.browsertoolbox.scope", "everything");
  await pushPref("dom.serviceWorkers.enabled", true);
  await pushPref("dom.serviceWorkers.testing.enabled", true);

  const ToolboxTask = await initBrowserToolboxTask();
  await ToolboxTask.importFunctions({
    waitUntil,
    waitForAllTargetsToBeAttached,
  });

  await addTab(`${EXAMPLE_URL}doc-all-workers.html`);

  await ToolboxTask.spawn(null, async () => {
    /* global gToolbox */
    await gToolbox.selectTool("jsdebugger");
    const dbg = gToolbox.getCurrentPanel().panelWin.dbg;

    await waitUntil(() => {
      const threads = dbg.selectors.getThreads();
      function hasWorker(workerName) {
        // eslint-disable-next-line max-nested-callbacks
        return threads.some(({ name }) => name == workerName);
      }
      return (
        hasWorker("simple-worker.js") &&
        hasWorker("shared-worker.js") &&
        hasWorker("service-worker.sjs")
      );
    });

    await waitForAllTargetsToBeAttached(gToolbox.commands.targetCommand);
  });
  ok(true, "All workers appear in browser toolbox debugger");

  invokeInTab("unregisterServiceWorker");

  await ToolboxTask.destroy();
});
