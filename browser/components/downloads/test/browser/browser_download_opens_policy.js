/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
  "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
);

ChromeUtils.defineModuleGetter(
  this,
  "DownloadsViewUI",
  "resource:///modules/DownloadsViewUI.jsm"
);

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const { DownloadIntegration } = ChromeUtils.import(
  "resource://gre/modules/DownloadIntegration.jsm"
);

add_task(async function test_download_opens_on_click() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.improvements_to_download_panel", true]],
  });
  Services.telemetry.clearScalars();

  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      ExemptDomainFileTypePairsFromFileTypeDownloadWarnings: [
        {
          file_extension: "jnlp",
          domains: ["localhost"],
        },
      ],
    },
  });

  startServer();
  mustInterruptResponses();
  let download = await promiseInterruptibleDownload(".jnlp");
  let publicList = await Downloads.getList(Downloads.PUBLIC);
  await publicList.add(download);

  let oldLaunchFile = DownloadIntegration.launchFile;

  let waitForLaunchFileCalled = new Promise(resolve => {
    DownloadIntegration.launchFile = () => {
      ok(true, "The file should be launched with an external application");
      resolve();
    };
  });

  registerCleanupFunction(async function() {
    DownloadIntegration.launchFile = oldLaunchFile;
    await task_resetState();
    Services.telemetry.clearScalars();
  });

  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent"),
    "downloads.file_opened",
    undefined,
    "File opened from panel should not be initialized"
  );

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

  continueResponses();
  await download.refresh();
  await promiseDownloadHasProgress(download, 100);

  await waitForLaunchFileCalled;

  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent"),
    "downloads.file_opened",
    1,
    "File opened from panel should be incremented"
  );
});
