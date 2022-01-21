/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gDownloadDir;
let downloads = [];

async function createDownloadFiles() {
  if (!gDownloadDir) {
    gDownloadDir = await setDownloadDir();
  }
  info("Created download directory: " + gDownloadDir);
  info("Setting path for download file");
  downloads.push({
    state: DownloadsCommon.DOWNLOAD_FINISHED,
    contentType: "text/plain",
    target: await createDownloadedFile(
      PathUtils.join(gDownloadDir, "downloaded.txt"),
      "Test file"
    ),
  });
  downloads.push({
    state: DownloadsCommon.DOWNLOAD_FINISHED,
    contentType: "text/javascript",
    target: await createDownloadedFile(
      PathUtils.join(gDownloadDir, "downloaded.js"),
      "Test file"
    ),
  });
}

registerCleanupFunction(async function() {
  await task_resetState();
});

add_task(async function test_download_deleteFile() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", true],
      ["browser.download.alwaysOpenPanel", false],
      ["browser.download.clearHistoryOnDelete", 2],
    ],
  });

  // remove download files, empty out collections
  let downloadList = await Downloads.getList(Downloads.ALL);
  let downloadCount = (await downloadList.getAll()).length;
  is(downloadCount, 0, "At the start of the test, there should be 0 downloads");
  await task_resetState();
  await createDownloadFiles();
  await task_addDownloads(downloads);
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

  let target1 = downloads[1].target;
  ok(target1.exists(), "downloaded.txt should exist");
  info(`file path: ${target1.path}`);
  let hiddenPromise = BrowserTestUtils.waitForEvent(
    DownloadsPanel.panel,
    "popuphidden"
  );

  contextMenu.activateItem(deleteFileItem);

  await TestUtils.waitForCondition(() => !target1.exists());

  await TestUtils.waitForCondition(() => {
    let downloadsListBox = document.getElementById("downloadsListBox");
    downloadsListBox.removeAttribute("disabled");
    return downloadsListBox.childElementCount == 1;
  });

  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.clearHistoryOnDelete", 0]],
  });
  info("trigger the context menu again");
  let itemTarget2 = document.querySelector(
    "#downloadsListBox richlistitem .downloadMainArea"
  );
  let contextMenu2 = await openContextMenu(itemTarget2);
  ok(
    !BrowserTestUtils.is_hidden(deleteFileItem),
    "deleteFileItem should be visible"
  );
  let target2 = downloads[0].target;
  ok(target2.exists(), "downloaded.js should exist");
  info(`file path: ${target2.path}`);
  contextMenu2.activateItem(deleteFileItem);
  await TestUtils.waitForCondition(() => !target2.exists());

  let downloadsListBox = document.getElementById("downloadsListBox");
  downloadsListBox.removeAttribute("disabled");
  Assert.greater(
    downloadsListBox.childElementCount,
    0,
    "There should be a download in the list"
  );

  DownloadsPanel.hidePanel();
  await hiddenPromise;
});
