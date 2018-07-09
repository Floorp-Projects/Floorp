/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineModuleGetter(this, "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm");

let win;

function waitForChildren(element, callback) {
  let MutationObserver = element.ownerGlobal.MutationObserver;
  return new Promise(resolve => {
    let observer = new MutationObserver(() => {
      if (callback()) {
        observer.disconnect();
        resolve();
      }
    });
    observer.observe(element, { childList: true });
  });
}

async function waitForChildrenLength(element, length, callback) {
  if (element.childElementCount != length) {
    await waitForChildren(element, () => element.childElementCount == length);
  }
}

registerCleanupFunction(async function() {
  await task_resetState();
  await PlacesUtils.history.clear();
});

async function testClearingDownloads(clearCallback) {
  const DOWNLOAD_DATA = [
    httpUrl("file1.txt"),
    httpUrl("file2.txt"),
    httpUrl("file3.txt"),
  ];

  let listbox = win.document.getElementById("downloadsRichListBox");
  ok(listbox, "download list box present");

  let promiseLength = waitForChildrenLength(listbox, DOWNLOAD_DATA.length);
  await simulateDropAndCheck(win, listbox, DOWNLOAD_DATA);
  await promiseLength;

  let receivedNotifications = [];
  let promiseNotification = PlacesTestUtils.waitForNotification("onDeleteURI",
    uri => {
      if (DOWNLOAD_DATA.includes(uri.spec)) {
        receivedNotifications.push(uri.spec);
      }
      return receivedNotifications.length == DOWNLOAD_DATA.length;
    }, "history");

  promiseLength = waitForChildrenLength(listbox, 0);
  await clearCallback(listbox);
  await promiseLength;

  await promiseNotification;

  Assert.deepEqual(receivedNotifications.sort(), DOWNLOAD_DATA.sort(),
    "Should have received notifications for each URL");
}

add_task(async function setup() {
  // Ensure that state is reset in case previous tests didn't finish.
  await task_resetState();

  await setDownloadDir();

  startServer();

  win = await openLibrary("Downloads");
  registerCleanupFunction(function() {
    win.close();
  });
});

add_task(async function test_clear_downloads_toolbar() {
  await testClearingDownloads(async () => {
    win.document.getElementById("clearDownloadsButton").click();
  });
});

add_task(async function test_clear_downloads_context_menu() {
  await testClearingDownloads(async listbox => {
    // Select one of the downloads.
    listbox.children[0].click();

    let contextMenu = win.document.getElementById("downloadsContextMenu");

    let popupShownPromise = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
    EventUtils.synthesizeMouseAtCenter(listbox.children[0], {type: "contextmenu", button: 2}, win);
    await popupShownPromise;

    // Find the clear context item.
    let clearDownloadsButton =
      [...contextMenu.children].find(child => child.command == "downloadsCmd_clearDownloads");
    clearDownloadsButton.click();
  });
});
