/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from head.js */
/* eslint no-unused-vars: [2, {"vars": "local"}] */

/**
 * Generate a test Task to record allocation when reloading a test page
 * while having one particular DevTools panel opened
 *
 * @param String recordName
 *        Name of the test recorded into PerfHerder/Talos database
 * @param String toolId
 *        ID of the panel to open
 */
function createPanelReloadTest(recordName, toolId) {
  return async function panelReloadTest() {
    const TEST_URL =
      "http://example.com/browser/devtools/client/framework/test/allocations/reloaded-page.html";

    async function testScript(toolbox) {
      const onTargetSwitched =
        toolbox.commands.targetCommand.once("switched-target");
      const onReloaded = toolbox.getCurrentPanel().once("reloaded");

      gBrowser.reloadTab(gBrowser.selectedTab);

      if (
        toolbox.commands.targetCommand.targetFront.targetForm
          .followWindowGlobalLifeCycle
      ) {
        info("Wait for target switched");
        await onTargetSwitched;
      }

      info("Wait for panel reload");
      await onReloaded;

      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      await new Promise(resolve => setTimeout(resolve, 1000));
    }

    const tab = await addTab(TEST_URL);

    const { require } = ChromeUtils.importESModule(
      "resource://devtools/shared/loader/Loader.sys.mjs"
    );
    const {
      gDevTools,
    } = require("resource://devtools/client/framework/devtools.js");
    const toolbox = await gDevTools.showToolboxForTab(tab, {
      toolId,
    });

    // Run the test scenario first before recording in order to load all the
    // modules. Otherwise they get reported as "still allocated" objects,
    // whereas we do expect them to be kept in memory as they are loaded via
    // the main DevTools loader, which keeps the module loaded until the
    // shutdown of Firefox
    await testScript(toolbox);
    // Running it a second time is helpful for the debugger which allocates different objects
    // on the second run... which would be taken as leak otherwise.
    await testScript(toolbox);

    await startRecordingAllocations({
      alsoRecordContentProcess: true,
    });

    // Now, run the test script. This time, we record this run.
    for (let i = 0; i < 10; i++) {
      await testScript(toolbox);
    }

    await stopRecordingAllocations(recordName, {
      alsoRecordContentProcess: true,
    });

    await toolbox.destroy();
    gBrowser.removeTab(tab);
  };
}
