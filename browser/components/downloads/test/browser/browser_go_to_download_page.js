/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const ReferrerInfo = Components.Constructor(
  "@mozilla.org/referrer-info;1",
  "nsIReferrerInfo",
  "init"
);

const TEST_REFERRER = "https://example.com/";

registerCleanupFunction(async function() {
  await task_resetState();
  await PlacesUtils.history.clear();
});

async function addDownload(referrerInfo) {
  let startTimeMs = Date.now();

  let publicList = await Downloads.getList(Downloads.PUBLIC);
  let downloadData = {
    source: {
      url: "http://www.example.com/test-download.txt",
      referrerInfo,
    },
    target: {
      path: gTestTargetFile.path,
    },
    startTime: new Date(startTimeMs++),
  };
  let download = await Downloads.createDownload(downloadData);
  await publicList.add(download);
  await download.start();
}

/**
 * Make sure "Go To Download Page" is enabled and works as expected.
 */
add_task(async function test_go_to_download_page() {
  let referrerInfo = new ReferrerInfo(
    Ci.nsIReferrerInfo.NO_REFERRER,
    true,
    NetUtil.newURI(TEST_REFERRER)
  );

  let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, TEST_REFERRER);

  // Wait for focus first
  await promiseFocus();

  // Ensure that state is reset in case previous tests didn't finish.
  await task_resetState();

  // Populate the downloads database with the data required by this test.
  await addDownload(referrerInfo);

  // Open the user interface and wait for data to be fully loaded.
  await task_openPanel();

  let win = await openLibrary("Downloads");
  registerCleanupFunction(function() {
    win.close();
  });

  let listbox = win.document.getElementById("downloadsRichListBox");
  ok(listbox, "download list box present");

  // Select one of the downloads.
  listbox.itemChildren[0].click();

  let contextMenu = win.document.getElementById("downloadsContextMenu");

  let popupShownPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(
    listbox.itemChildren[0],
    { type: "contextmenu", button: 2 },
    win
  );
  await popupShownPromise;

  // Find and click "Go To Download Page"
  let goToDownloadButton = [...contextMenu.children].find(
    child => child.command == "downloadsCmd_openReferrer"
  );
  goToDownloadButton.click();

  let newTab = await tabPromise;
  ok(newTab, "Go To Download Page opened a new tab");
  gBrowser.removeTab(newTab);
});
