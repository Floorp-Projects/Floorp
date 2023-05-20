/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Record allocations while opening and closing DevTools

const TEST_URL =
  "data:text/html;charset=UTF-8,<div>Target allocations test</div>";

const { require } = ChromeUtils.importESModule(
  "resource://devtools/shared/loader/Loader.sys.mjs"
);
const {
  gDevTools,
} = require("resource://devtools/client/framework/devtools.js");

async function testScript(tab) {
  const toolbox = await gDevTools.showToolboxForTab(tab, {
    toolId: "inspector",
  });

  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 1000));

  await toolbox.destroy();

  // Spin the event loop to ensure toolbox destroy is fully completed
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 0));
}

add_task(async function () {
  const tab = await addTab(TEST_URL);

  // Run the test scenario first before recording in order to load all the
  // modules. Otherwise they get reported as "still allocated" objects,
  // whereas we do expect them to be kept in memory as they are loaded via
  // the main DevTools loader, which keeps the module loaded until the
  // shutdown of Firefox
  await testScript(tab);

  await startRecordingAllocations();

  // Now, run the test script. This time, we record this run.
  for (let i = 0; i < 3; i++) {
    await testScript(tab);
  }

  await stopRecordingAllocations("toolbox");

  gBrowser.removeTab(tab);
});
