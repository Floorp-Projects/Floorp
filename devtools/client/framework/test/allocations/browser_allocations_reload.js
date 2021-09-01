/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Record allocations while reloading the page with the DevTools opened

const TEST_URL =
  "data:text/html;charset=UTF-8,<div>Target allocations test</div>";

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
const { gDevTools } = require("devtools/client/framework/devtools");

async function testScript(toolbox) {
  const onTargetSwitched = toolbox.commands.targetCommand.once(
    "switched-target"
  );
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

add_task(async function() {
  const tab = await addTab(TEST_URL);
  const toolbox = await gDevTools.showToolboxForTab(tab, {
    toolId: "inspector",
  });

  // Run the test scenario first before recording in order to load all the
  // modules. Otherwise they get reported as "still allocated" objects,
  // whereas we do expect them to be kept in memory as they are loaded via
  // the main DevTools loader, which keeps the module loaded until the
  // shutdown of Firefox
  await testScript(toolbox);

  const recordData = await startRecordingAllocations({
    alsoRecordContentProcess: true,
  });

  // Now, run the test script. This time, we record this run.
  for (let i = 0; i < 10; i++) {
    await testScript(toolbox);
  }

  await stopRecordingAllocations(recordData, "reload");

  await toolbox.destroy();
  gBrowser.removeTab(tab);
});
