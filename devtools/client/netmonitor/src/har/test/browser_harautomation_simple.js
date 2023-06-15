/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const HAR_FILENAME = "test_filename.har";

// We expect the HAR file to be created on reload in profD/har/logs
const HAR_PATH = ["har", "logs", HAR_FILENAME];

/**
 * Smoke test for automated HAR export.
 * Note that the `enableAutoExportToFile` is set from browser-harautomation.ini
 * because the preference needs to be set before starting the browser.
 */
add_task(async function () {
  // Set a simple test filename for the exported HAR.
  await pushPref("devtools.netmonitor.har.defaultFileName", "test_filename");

  const tab = await addTab(SIMPLE_URL);
  const toolbox = await gDevTools.showToolboxForTab(tab, {
    toolId: "inspector",
  });

  await reloadBrowser();

  info("Wait until the HAR file is created in the profile directory");
  const harFile = new FileUtils.File(
    PathUtils.join(PathUtils.profileDir, ...HAR_PATH)
  );

  await waitUntil(() => harFile.exists());
  ok(harFile.exists(), "HAR file was automatically created");

  await toolbox.destroy();
  await removeTab(tab);
});
