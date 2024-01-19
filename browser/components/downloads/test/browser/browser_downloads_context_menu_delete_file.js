/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var { DownloadHistory } = ChromeUtils.importESModule(
  "resource://gre/modules/DownloadHistory.sys.mjs"
);
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

add_setup(startServer);

registerCleanupFunction(async function () {
  await task_resetState();
  await PlacesUtils.history.clear();
});

add_task(async function test_download_deleteFile() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.always_ask_before_handling_new_types", false],
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
    !BrowserTestUtils.isHidden(deleteFileItem),
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
    !BrowserTestUtils.isHidden(deleteFileItem),
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

  ok(
    !DownloadsView.richListBox.selectedItem._shell.isCommandEnabled(
      "downloadsCmd_deleteFile"
    ),
    "Delete file command should be disabled"
  );

  DownloadsPanel.hidePanel();
  await hiddenPromise;
});

add_task(async function test_about_downloads_deleteFile_for_history_download() {
  await task_resetState();
  await PlacesUtils.history.clear();

  if (!gDownloadDir) {
    gDownloadDir = await setDownloadDir();
  }

  let targetFile = await createDownloadedFile(
    PathUtils.join(gDownloadDir, "test-download.txt"),
    "blah blah blah"
  );
  let endTime;
  try {
    endTime = targetFile.creationTime;
  } catch (e) {
    endTime = Date.now();
  }
  let download = {
    source: {
      url: httpUrl(targetFile.leafName),
      isPrivate: false,
    },
    target: {
      path: targetFile.path,
      size: targetFile.fileSize,
    },
    succeeded: true,
    stopped: true,
    endTime,
    fileSize: targetFile.fileSize,
    state: 1,
  };

  function promiseWaitForVisit(aUrl) {
    return new Promise(resolve => {
      function listener(aEvents) {
        Assert.equal(aEvents.length, 1);
        let event = aEvents[0];
        Assert.equal(event.type, "page-visited");
        if (event.url == aUrl) {
          PlacesObservers.removeListener(["page-visited"], listener);
          resolve([
            event.visitTime,
            event.transitionType,
            event.lastKnownTitle,
          ]);
        }
      }
      PlacesObservers.addListener(["page-visited"], listener);
    });
  }

  function waitForAnnotation(sourceUriSpec, annotationName) {
    return TestUtils.waitForCondition(async () => {
      let pageInfo = await PlacesUtils.history.fetch(sourceUriSpec, {
        includeAnnotations: true,
      });
      return pageInfo && pageInfo.annotations.has(annotationName);
    }, `Should have found annotation ${annotationName} for ${sourceUriSpec}.`);
  }

  // Add the download to history using the XPCOM service, then use the
  // DownloadHistory module to save the associated metadata.
  let promiseFileAnnotation = waitForAnnotation(
    download.source.url,
    "downloads/destinationFileURI"
  );
  let promiseMetaAnnotation = waitForAnnotation(
    download.source.url,
    "downloads/metaData"
  );
  let promiseVisit = promiseWaitForVisit(download.source.url);
  await DownloadHistory.addDownloadToHistory(download);
  await promiseVisit;
  await DownloadHistory.updateMetaData(download);
  await Promise.all([promiseFileAnnotation, promiseMetaAnnotation]);

  let win = await openLibrary("Downloads");
  registerCleanupFunction(function () {
    win?.close();
  });

  let box = win.document.getElementById("downloadsListBox");
  ok(box, "Should have list of downloads");
  is(box.children.length, 1, "Should have 1 download.");
  let kid = box.firstChild;
  let desc = kid.querySelector(".downloadTarget");
  let dl = kid._shell.download;
  // This would just be an `is` check, but stray temp files
  // if this test (or another in this dir) ever fails could throw that off.
  ok(
    desc.value.includes("test-download"),
    `Label '${desc.value}' should include 'test-download'`
  );
  ok(kid.selected, "First item should be selected.");
  ok(dl.placesNode, "Download should have history.");
  ok(targetFile.exists(), "Download target should exist.");
  let contextMenu = win.document.getElementById("downloadsContextMenu");
  let popupShownPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(
    kid,
    { type: "contextmenu", button: 2 },
    win
  );
  await popupShownPromise;
  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  contextMenu.activateItem(
    contextMenu.querySelector(".downloadDeleteFileMenuItem")
  );
  await popupHiddenPromise;
  await TestUtils.waitForCondition(() => !targetFile.exists());
  info("History download target deleted.");
});
