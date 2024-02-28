/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

/**
 * Tests if saving a response to a file works..
 */

add_task(async function () {
  const { tab, monitor } = await initNetMonitor(
    CONTENT_TYPE_WITHOUT_CACHE_URL,
    { requestCount: 1 }
  );
  info("Starting test... ");

  const { document } = monitor.panelWin;

  // Execute requests.
  await performRequests(monitor, tab, CONTENT_TYPE_WITHOUT_CACHE_REQUESTS);

  // Create the folder the gzip file will be saved into
  const destDir = createTemporarySaveDirectory();
  let destFile;

  MockFilePicker.displayDirectory = destDir;
  const saveDialogClosedPromise = new Promise(resolve => {
    MockFilePicker.showCallback = function (fp) {
      info("MockFilePicker showCallback");
      const fileName = fp.defaultString;
      destFile = destDir.clone();
      destFile.append(fileName);
      MockFilePicker.setFiles([destFile]);

      resolve(destFile.path);
    };
  });

  registerCleanupFunction(function () {
    MockFilePicker.cleanup();
    destDir.remove(true);
  });

  // Select gzip request.

  info("Open the context menu");

  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[6]
  );

  EventUtils.sendMouseEvent(
    { type: "contextmenu" },
    document.querySelectorAll(".request-list-item")[6]
  );

  info("Open the save dialog");
  await selectContextMenuItem(monitor, "request-list-context-save-response-as");

  info("Wait for the save dialog to close");
  const savedPath = await saveDialogClosedPromise;

  const expectedFile = destDir.clone();
  expectedFile.append("sjs_content-type-test-server.sjs");

  is(savedPath, expectedFile.path, "Response was saved to correct path");

  info("Wait for the downloaded file to be fully saved to disk: " + savedPath);
  await TestUtils.waitForCondition(async () => {
    if (!(await IOUtils.exists(savedPath))) {
      return false;
    }
    const { size } = await IOUtils.stat(savedPath);
    return size > 0;
  });

  const buffer = await IOUtils.read(savedPath);
  const savedFileContent = new TextDecoder().decode(buffer);

  // The content is set by https://searchfox.org/mozilla-central/source/devtools/client/netmonitor/test/sjs_content-type-test-server.sjs#360
  // (the "gzip" case)
  is(
    savedFileContent,
    new Array(1000).join("Hello gzip!"),
    "Saved response has the correct text"
  );

  await teardown(monitor);
});

function createTemporarySaveDirectory() {
  const saveDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  saveDir.append("testsavedir");
  if (!saveDir.exists()) {
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  }
  return saveDir;
}
