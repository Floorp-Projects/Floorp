/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  DownloadsViewUI: "resource:///modules/DownloadsViewUI.sys.mjs",
});

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

add_task(async function test_download_clickable() {
  Services.telemetry.clearScalars();

  startServer();
  mustInterruptResponses();
  let download = await promiseInterruptibleDownload();
  let publicList = await Downloads.getList(Downloads.PUBLIC);
  await publicList.add(download);

  registerCleanupFunction(async function () {
    await task_resetState();
    Services.telemetry.clearScalars();
  });

  download.start();

  await promiseDownloadHasProgress(download, 50);

  await task_openPanel();

  let listbox = document.getElementById("downloadsListBox");
  ok(listbox, "Download list box present");

  await TestUtils.waitForCondition(() => {
    return listbox.childElementCount == 1;
  });

  info("All downloads show in the listbox.itemChildren ", listbox.itemChildren);

  ok(
    listbox.itemChildren[0].classList.contains("openWhenFinished"),
    "Download should have clickable style when in progress"
  );

  ok(!download.launchWhenSucceeded, "launchWhenSucceeded should set to false");

  ok(!download._launchedFromPanel, "LaunchFromPanel should set to false");

  EventUtils.synthesizeMouseAtCenter(listbox.itemChildren[0], {});
  ok(
    download.launchWhenSucceeded,
    "Should open the file when download is finished"
  );
  ok(download._launchedFromPanel, "File was scheduled to launch from panel");

  EventUtils.synthesizeMouseAtCenter(listbox.itemChildren[0], {});

  ok(
    !download.launchWhenSucceeded,
    "Should NOT open the file when download is finished"
  );

  ok(!download._launchedFromPanel, "File launch from panel was reset");

  continueResponses();
  await download.refresh();
  await promiseDownloadHasProgress(download, 100);

  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent"),
    "downloads.file_opened",
    undefined,
    "File opened from panel should not be incremented"
  );
});
