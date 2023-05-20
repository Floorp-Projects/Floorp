/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Record allocations while reloading the page without anything related to DevTools running

const TEST_URL =
  "http://example.com/browser/devtools/client/framework/test/allocations/reloaded-page.html";

async function testScript() {
  await BrowserTestUtils.reloadTab(gBrowser.selectedTab);

  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 1000));
}

add_task(async function () {
  const tab = await addTab(TEST_URL);

  // Run the test scenario first before recording in order to load all the
  // modules. Otherwise they get reported as "still allocated" objects,
  // whereas we do expect them to be kept in memory as they are loaded via
  // the main DevTools loader, which keeps the module loaded until the
  // shutdown of Firefox
  await testScript();

  await startRecordingAllocations({
    alsoRecordContentProcess: true,
  });

  // Now, run the test script. This time, we record this run.
  for (let i = 0; i < 10; i++) {
    await testScript();
  }

  await stopRecordingAllocations("reload-no-devtools", {
    alsoRecordContentProcess: true,
  });

  gBrowser.removeTab(tab);
});
