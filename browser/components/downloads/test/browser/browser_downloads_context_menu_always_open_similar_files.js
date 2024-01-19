/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gMimeSvc = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
let gHandlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
  Ci.nsIHandlerService
);

let gDownloadDir;
const TestFiles = {};
let downloads = [];
const { handleInternally, saveToDisk, useSystemDefault, alwaysAsk } =
  Ci.nsIHandlerInfo;

function ensureMIMEState({ preferredAction, alwaysAskBeforeHandling = false }) {
  const mimeInfo = gMimeSvc.getFromTypeAndExtension("text/plain", "txt");
  mimeInfo.preferredAction = preferredAction;
  mimeInfo.alwaysAskBeforeHandling = alwaysAskBeforeHandling;
  gHandlerSvc.store(mimeInfo);
}

async function createDownloadFile() {
  if (!gDownloadDir) {
    gDownloadDir = await setDownloadDir();
  }
  info("Created download directory: " + gDownloadDir);
  TestFiles.txt = await createDownloadedFile(
    PathUtils.join(gDownloadDir, "downloaded.txt"),
    "Test file"
  );
  info("Created downloaded text file at:" + TestFiles.txt.path);

  info("Setting path for download file");
  // Set target for download file. Otherwise, file will default to .file instead of txt
  // when we prepare our downloads - particularly in task_addDownloads().
  let targetPath = PathUtils.join(PathUtils.tempDir, "downloaded.txt");
  let target = new FileUtils.File(targetPath);
  target.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
  downloads.push({
    state: DownloadsCommon.DOWNLOAD_FINISHED,
    contentType: "text/plain",
    target,
  });
}

async function prepareDownloadFiles(downloadList) {
  // prepare downloads
  await task_addDownloads(downloads);
  let [download] = await downloadList.getAll();
  info("Download succeeded? " + download.succeeded);
  info("Download target exists? " + download.target.exists);
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.always_ask_before_handling_new_types", false]],
  });
  const originalOpenDownload = DownloadsCommon.openDownload;
  // overwrite DownloadsCommon.openDownload to prevent file from opening during tests
  DownloadsCommon.openDownload = async () => {
    info("Overwriting openDownload for tests");
  };

  registerCleanupFunction(async () => {
    DownloadsCommon.openDownload = originalOpenDownload;
    info("Resetting downloads and closing downloads panel");
    await task_resetState();
  });

  // remove download files, empty out collections
  let downloadList = await Downloads.getList(Downloads.ALL);
  let downloadCount = (await downloadList.getAll()).length;
  is(downloadCount, 0, "At the start of the test, there should be 0 downloads");

  await createDownloadFile();
  await prepareDownloadFiles(downloadList);
});

add_task(async function test_checkbox_useSystemDefault() {
  // force mimetype pref
  ensureMIMEState({ preferredAction: useSystemDefault });

  await task_openPanel();
  await TestUtils.waitForCondition(() => {
    let downloadsListBox = document.getElementById("downloadsListBox");
    downloadsListBox.removeAttribute("disabled");
    return downloadsListBox.childElementCount == downloads.length;
  });

  info("trigger the context menu");
  let itemTarget = document.querySelector(
    "#downloadsListBox richlistitem .downloadMainArea"
  );

  let contextMenu = await openContextMenu(itemTarget);
  let alwaysOpenSimilarFilesItem = contextMenu.querySelector(
    ".downloadAlwaysOpenSimilarFilesMenuItem"
  );

  ok(
    !BrowserTestUtils.isHidden(alwaysOpenSimilarFilesItem),
    "alwaysOpenSimilarFiles should be visible"
  );
  ok(
    alwaysOpenSimilarFilesItem.hasAttribute("checked"),
    "alwaysOpenSimilarFiles should have checkbox attribute"
  );

  contextMenu.hidePopup();
  let hiddenPromise = BrowserTestUtils.waitForEvent(
    DownloadsPanel.panel,
    "popuphidden"
  );
  DownloadsPanel.hidePanel();
  await hiddenPromise;
});

add_task(async function test_checkbox_saveToDisk() {
  // force mimetype pref
  ensureMIMEState({ preferredAction: saveToDisk });

  await task_openPanel();
  await TestUtils.waitForCondition(() => {
    let downloadsListBox = document.getElementById("downloadsListBox");
    downloadsListBox.removeAttribute("disabled");
    return downloadsListBox.childElementCount == downloads.length;
  });

  info("trigger the context menu");
  let itemTarget = document.querySelector(
    "#downloadsListBox richlistitem .downloadMainArea"
  );

  let contextMenu = await openContextMenu(itemTarget);
  let alwaysOpenSimilarFilesItem = contextMenu.querySelector(
    ".downloadAlwaysOpenSimilarFilesMenuItem"
  );

  ok(
    !BrowserTestUtils.isHidden(alwaysOpenSimilarFilesItem),
    "alwaysOpenSimilarFiles should be visible"
  );
  ok(
    !alwaysOpenSimilarFilesItem.hasAttribute("checked"),
    "alwaysOpenSimilarFiles should not have checkbox attribute"
  );

  contextMenu.hidePopup();
  let hiddenPromise = BrowserTestUtils.waitForEvent(
    DownloadsPanel.panel,
    "popuphidden"
  );
  DownloadsPanel.hidePanel();
  await hiddenPromise;
});

add_task(async function test_preferences_enable_alwaysOpenSimilarFiles() {
  // Force mimetype pref
  ensureMIMEState({ preferredAction: saveToDisk });

  // open panel
  await task_openPanel();
  await TestUtils.waitForCondition(() => {
    let downloadsListBox = document.getElementById("downloadsListBox");
    downloadsListBox.removeAttribute("disabled");
    return downloadsListBox.childElementCount == downloads.length;
  });

  info("trigger the context menu");
  let itemTarget = document.querySelector(
    "#downloadsListBox richlistitem .downloadMainArea"
  );

  let contextMenu = await openContextMenu(itemTarget);
  let alwaysOpenSimilarFilesItem = contextMenu.querySelector(
    ".downloadAlwaysOpenSimilarFilesMenuItem"
  );

  alwaysOpenSimilarFilesItem.click();

  await TestUtils.waitForCondition(() => {
    let mimeInfo = gMimeSvc.getFromTypeAndExtension("text/plain", "txt");
    return mimeInfo.preferredAction === useSystemDefault;
  });
  let mimeInfo = gMimeSvc.getFromTypeAndExtension("text/plain", "txt");

  is(
    mimeInfo.preferredAction,
    useSystemDefault,
    "Preference should switch to useSystemDefault"
  );

  contextMenu.hidePopup();
  DownloadsPanel.hidePanel();
});

add_task(async function test_preferences_disable_alwaysOpenSimilarFiles() {
  // Force mimetype pref
  ensureMIMEState({ preferredAction: useSystemDefault });

  await task_openPanel();
  await TestUtils.waitForCondition(() => {
    let downloadsListBox = document.getElementById("downloadsListBox");
    downloadsListBox.removeAttribute("disabled");
    return downloadsListBox.childElementCount == downloads.length;
  });

  info("trigger the context menu");
  let itemTarget = document.querySelector(
    "#downloadsListBox richlistitem .downloadMainArea"
  );

  let contextMenu = await openContextMenu(itemTarget);
  let alwaysOpenSimilarFilesItem = contextMenu.querySelector(
    ".downloadAlwaysOpenSimilarFilesMenuItem"
  );

  alwaysOpenSimilarFilesItem.click();

  await TestUtils.waitForCondition(() => {
    let mimeInfo = gMimeSvc.getFromTypeAndExtension("text/plain", "txt");
    return mimeInfo.preferredAction === saveToDisk;
  });
  let mimeInfo = gMimeSvc.getFromTypeAndExtension("text/plain", "txt");

  is(
    mimeInfo.preferredAction,
    saveToDisk,
    "Preference should switch to saveToDisk"
  );

  contextMenu.hidePopup();
  DownloadsPanel.hidePanel();
});
