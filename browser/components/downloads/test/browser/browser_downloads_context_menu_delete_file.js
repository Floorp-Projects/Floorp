/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gDownloadDir;
const TestFiles = {};
let downloads = [];

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
  let target = FileUtils.getFile("TmpD", ["downloaded.txt"]);
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

add_task(async function test_download_deleteFile() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.improvements_to_download_panel", true]],
  });

  // remove download files, empty out collections
  let downloadList = await Downloads.getList(Downloads.ALL);
  let downloadCount = (await downloadList.getAll()).length;
  is(downloadCount, 0, "At the start of the test, there should be 0 downloads");
  await task_resetState();
  await createDownloadFile();
  await prepareDownloadFiles(downloadList);
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
  let deleteFileItem = contextMenu.querySelector(
    '[command="downloadsCmd_deleteFile"]'
  );

  ok(
    !BrowserTestUtils.is_hidden(deleteFileItem),
    "deleteFileItem should be visible"
  );

  let target = FileUtils.getFile("TmpD", ["downloaded.txt"]);
  ok(target.exists(), "downloaded.txt should exist");
  info(`file path: ${target.path}`);
  let hiddenPromise = BrowserTestUtils.waitForEvent(
    DownloadsPanel.panel,
    "popuphidden"
  );

  deleteFileItem.click();

  await TestUtils.waitForCondition(() => !target.exists());

  await TestUtils.waitForCondition(() => {
    let downloadsListBox = document.getElementById("downloadsListBox");
    downloadsListBox.removeAttribute("disabled");
    return downloadsListBox.childElementCount == 0;
  });
  DownloadsPanel.hidePanel();
  await hiddenPromise;
});
