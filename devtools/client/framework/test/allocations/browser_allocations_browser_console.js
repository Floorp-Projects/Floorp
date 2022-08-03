/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Record allocations while opening and closing the Browser Console

const TEST_URL =
  "http://example.com/browser/devtools/client/framework/test/allocations/reloaded-page.html";

const { require } = ChromeUtils.import(
  "resource://devtools/shared/loader/Loader.jsm"
);
const {
  BrowserConsoleManager,
} = require("devtools/client/webconsole/browser-console-manager");

async function testScript() {
  // Open
  await BrowserConsoleManager.toggleBrowserConsole();

  // Reload the tab to make the test slightly more real
  const hud = BrowserConsoleManager.getBrowserConsole();
  const onTargetProcessed = hud.commands.targetCommand.once(
    "processed-available-target"
  );

  gBrowser.reloadTab(gBrowser.selectedTab);

  info("Wait for target of the new document to be fully processed");
  await onTargetProcessed;

  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 1000));

  // Close
  await BrowserConsoleManager.toggleBrowserConsole();

  // Browser console still cleanup stuff after the resolution of toggleBrowserConsole.
  // So wait for a little while to ensure it completes all cleanups.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 500));
}

add_task(async function() {
  // We only want to test the multiprocess browser console,
  // even on beta and release.
  await SpecialPowers.pushPrefEnv({
    set: [["devtools.browsertoolbox.fission", true]],
  });
  await SpecialPowers.pushPrefEnv({
    set: [["devtools.browsertoolbox.scope", "everything"]],
  });

  const tab = await addTab(TEST_URL);

  // Run the test scenario first before recording in order to load all the
  // modules. Otherwise they get reported as "still allocated" objects,
  // whereas we do expect them to be kept in memory as they are loaded via
  // the main DevTools loader, which keeps the module loaded until the
  // shutdown of Firefox
  await testScript();

  // Now, run the test script. This time, we record this run.
  await startRecordingAllocations();

  for (let i = 0; i < 3; i++) {
    await testScript();
  }

  await stopRecordingAllocations("browser-console");

  gBrowser.removeTab(tab);
});
