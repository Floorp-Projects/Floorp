/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const UCT_URI = "chrome://mozapps/content/downloads/unknownContentType.xhtml";
// Need to start the server before `httpUrl` works.
startServer();
const DOWNLOAD_URL = httpUrl("interruptible.txt");

let gDownloadDir;

let gExternalHelperAppService = Cc[
  "@mozilla.org/uriloader/external-helper-app-service;1"
].getService(Ci.nsIExternalHelperAppService);
gExternalHelperAppService.QueryInterface(Ci.nsIObserver);

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.start_downloads_in_tmp_dir", true],
      ["browser.helperApps.deleteTempFileOnExit", true],
    ],
  });
  registerCleanupFunction(task_resetState);
  gDownloadDir = new FileUtils.File(await setDownloadDir());
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.download.dir");
    Services.prefs.clearUserPref("browser.download.folderList");
  });
});

add_task(async function test_download_asking_starts_in_tmp() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.always_ask_before_handling_new_types", true]],
  });
  let list = await Downloads.getList(Downloads.PUBLIC);
  let downloadStarted = new Promise(resolve => {
    let view = {
      onDownloadAdded(download) {
        list.removeView(view);
        resolve(download);
      },
    };
    list.addView(view);
  });
  // Wait for the download prompting dialog
  let dialogPromise = BrowserTestUtils.domWindowOpenedAndLoaded(
    null,
    win => win.document.documentURI == UCT_URI
  );
  serveInterruptibleAsDownload();
  mustInterruptResponses();
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: DOWNLOAD_URL,
      waitForLoad: false,
      waitForStop: true,
    },
    async function () {
      let dialogWin = await dialogPromise;
      let tempFile = dialogWin.dialog.mLauncher.targetFile;
      ok(
        !tempFile.parent.equals(gDownloadDir),
        "Should not have put temp file in the downloads dir."
      );

      let dialogEl = dialogWin.document.querySelector("dialog");
      dialogEl.getButton("accept").disabled = false;
      dialogEl.acceptDialog();
      let download = await downloadStarted;
      is(
        PathUtils.parent(download.target.path),
        gDownloadDir.path,
        "Should have put final file in the downloads dir."
      );
      continueResponses();
      await download.whenSucceeded();
      await IOUtils.remove(download.target.path);
    }
  );
  await list.removeFinished();
});

add_task(async function test_download_asking_and_opening_opens_from_tmp() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.always_ask_before_handling_new_types", true]],
  });
  let list = await Downloads.getList(Downloads.PUBLIC);
  let downloadStarted = new Promise(resolve => {
    let view = {
      onDownloadAdded(download) {
        list.removeView(view);
        resolve(download);
      },
    };
    list.addView(view);
  });
  // Wait for the download prompting dialog
  let dialogPromise = BrowserTestUtils.domWindowOpenedAndLoaded(
    null,
    win => win.document.documentURI == UCT_URI
  );

  let oldLaunchFile = DownloadIntegration.launchFile;
  let promiseLaunchFileCalled = new Promise(resolve => {
    DownloadIntegration.launchFile = file => {
      ok(true, "The file should be launched with an external application");
      resolve(file);
      DownloadIntegration.launchFile = oldLaunchFile;
    };
  });
  registerCleanupFunction(() => {
    DownloadIntegration.launchFile = oldLaunchFile;
  });

  serveInterruptibleAsDownload();
  mustInterruptResponses();
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: DOWNLOAD_URL,
      waitForLoad: false,
      waitForStop: true,
    },
    async function () {
      let dialogWin = await dialogPromise;
      let tempFile = dialogWin.dialog.mLauncher.targetFile;
      ok(
        !tempFile.parent.equals(gDownloadDir),
        "Should not have put temp file in the downloads dir."
      );

      dialogWin.document.getElementById("open").click();
      let dialogEl = dialogWin.document.querySelector("dialog");
      dialogEl.getButton("accept").disabled = false;
      dialogEl.acceptDialog();
      let download = await downloadStarted;
      isnot(
        PathUtils.parent(download.target.path),
        gDownloadDir.path,
        "Should not have put final file in the downloads dir when it's supposed to be automatically opened."
      );
      continueResponses();
      await download.whenSucceeded();
      await download.refresh();
      isnot(
        PathUtils.parent(download.target.path),
        gDownloadDir.path,
        "Once finished the download should not be in the downloads dir when it's supposed to be automatically opened."
      );
      let file = await promiseLaunchFileCalled;
      ok(
        !file.parent.equals(gDownloadDir),
        "Should not have put opened file in the downloads dir."
      );

      // Pretend that we've quit so we wipe all the files:
      gExternalHelperAppService.observe(null, "profile-before-change", "");
      // Now the file should go away, but that's async...

      let f = new FileUtils.File(download.target.path);
      await TestUtils.waitForCondition(
        () => !f.exists(),
        "Temp file should be removed",
        500
      ).catch(err => ok(false, err));
      ok(!f.exists(), "Temp file should be removed.");

      await IOUtils.remove(download.target.path);
    }
  );
  await list.removeFinished();
});

// Check that if we open the file automatically, it opens from the temp dir.
add_task(async function test_download_automatically_opened_from_tmp() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.always_ask_before_handling_new_types", false]],
  });
  serveInterruptibleAsDownload();
  mustInterruptResponses();

  let list = await Downloads.getList(Downloads.PUBLIC);
  let downloadStarted = new Promise(resolve => {
    let view = {
      onDownloadAdded(download) {
        list.removeView(view);
        resolve(download);
      },
    };
    list.addView(view);
  });

  const handlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
    Ci.nsIHandlerService
  );
  const mimeSvc = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);

  let txtHandlerInfo = mimeSvc.getFromTypeAndExtension("text/plain", "txt");
  txtHandlerInfo.preferredAction = Ci.nsIHandlerInfo.useSystemDefault;
  txtHandlerInfo.alwaysAskBeforeHandling = false;
  handlerSvc.store(txtHandlerInfo);
  registerCleanupFunction(() => handlerSvc.remove(txtHandlerInfo));

  let oldLaunchFile = DownloadIntegration.launchFile;
  let promiseLaunchFileCalled = new Promise(resolve => {
    DownloadIntegration.launchFile = file => {
      ok(true, "The file should be launched with an external application");
      resolve(file);
      DownloadIntegration.launchFile = oldLaunchFile;
    };
  });
  registerCleanupFunction(() => {
    DownloadIntegration.launchFile = oldLaunchFile;
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: DOWNLOAD_URL,
      waitForLoad: false,
      waitForStop: true,
    },
    async function () {
      let download = await downloadStarted;
      isnot(
        PathUtils.parent(download.target.partFilePath),
        gDownloadDir.path,
        "Should not start the download in the downloads dir."
      );
      continueResponses();
      await download.whenSucceeded();
      isnot(
        PathUtils.parent(download.target.path),
        gDownloadDir.path,
        "Should not have put final file in the downloads dir."
      );
      let file = await promiseLaunchFileCalled;
      ok(
        !file.parent.equals(gDownloadDir),
        "Should not have put opened file in the downloads dir."
      );

      // Pretend that we've quit so we wipe all the files:
      gExternalHelperAppService.observe(null, "profile-before-change", "");
      // Now the file should go away, but that's async...

      let f = new FileUtils.File(download.target.path);
      await TestUtils.waitForCondition(
        () => !f.exists(),
        "Temp file should be removed",
        500
      ).catch(err => ok(false, err));
      ok(!f.exists(), "Temp file should be removed.");

      await IOUtils.remove(download.target.path);
    }
  );

  handlerSvc.remove(txtHandlerInfo);
  await list.removeFinished();
});
