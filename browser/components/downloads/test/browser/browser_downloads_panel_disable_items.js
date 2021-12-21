/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "https://example.com";
const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  TEST_URI
);

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", true],
      ["security.dialog_enable_delay", 1000],
    ],
  });
  // Remove download files from previous tests
  await task_resetState();
  let downloadList = await Downloads.getList(Downloads.ALL);
  let downloadCount = (await downloadList.getAll()).length;
  is(downloadCount, 0, "At the start of the test, there should be 0 downloads");
});

/**
 * Tests that the download items remain enabled when we manually open
 * the downloads panel by clicking the downloads button.
 */
add_task(async function test_downloads_panel_downloads_button() {
  let panelOpenedPromise = promisePanelOpened();
  await task_addDownloads([{ state: DownloadsCommon.DOWNLOAD_FINISHED }]);
  await panelOpenedPromise;

  // The downloads panel may open automatically after task_addDownloads
  // creates a download file. Let's close the panel and reopen it again
  // (but this time manually) to ensure the download items are not disabled.
  DownloadsPanel.hidePanel();

  ok(!DownloadsPanel.isPanelShowing, "Downloads Panel should not be visible");

  info("Manually open the download panel to view list of downloads");
  let downloadsButton = document.getElementById("downloads-button");
  EventUtils.synthesizeMouseAtCenter(downloadsButton, {});
  let downloadsListBox = document.getElementById("downloadsListBox");

  ok(downloadsListBox, "downloadsListBox richlistitem should be visible");
  is(
    downloadsListBox.childElementCount,
    1,
    "downloadsListBox should have 1 download"
  );
  ok(
    !downloadsListBox.getAttribute("disabled"),
    "All download items in the downloads panel should not be disabled"
  );

  info("Cleaning up downloads");
  await task_resetState();
});

/**
 * Tests that the download items are disabled when the downloads panel is
 * automatically opened as a result of a new download.
 */
add_task(async function test_downloads_panel_new_download() {
  // Overwrite DownloadsCommon.openDownload to prevent file from opening during tests
  const originalOpenDownload = DownloadsCommon.openDownload;
  DownloadsCommon.openDownload = async () => {
    ok(false, "openDownload was called when it was not expected");
  };
  let newTabPromise = BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PATH + "foo.txt"
  );
  await promisePanelOpened();
  let downloadsListBox = document.getElementById("downloadsListBox");

  ok(downloadsListBox, "downloadsListBox richlistitem should be visible");
  await BrowserTestUtils.waitForMutationCondition(
    downloadsListBox,
    { childList: true },
    () => downloadsListBox.childElementCount == 1
  );
  info("downloadsListBox should have 1 download");
  ok(
    downloadsListBox.getAttribute("disabled"),
    "All download items in the downloads panel should first be disabled"
  );

  let newTab = await newTabPromise;

  EventUtils.synthesizeKey("KEY_Enter", {}, window);
  DownloadsCommon.openDownload = async () => {
    ok(true, "openDownload should have been called");
  };

  await BrowserTestUtils.waitForCondition(
    () => !downloadsListBox.getAttribute("disabled")
  );
  info("All download items in the download panel should now be enabled");
  EventUtils.synthesizeKey("KEY_Enter", {}, window);

  await task_resetState();
  DownloadsCommon.openDownload = originalOpenDownload;
  BrowserTestUtils.removeTab(newTab);
});

/**
 * Tests that the disabled attribute does not exist when we close the
 * downloads panel before the disabled state timeout resolves.
 */
add_task(async function test_downloads_panel_close_panel_early() {
  info("Creating mock completed downloads");
  await task_addDownloads([{ state: DownloadsCommon.DOWNLOAD_FINISHED }]);

  // The downloads panel may open automatically after task_addDownloads
  // creates a download file. Let's close the panel and reopen it again
  // (but this time manually).
  DownloadsPanel.hidePanel();

  ok(!DownloadsPanel.isPanelShowing, "Downloads Panel should not be visible");

  info("Manually open the download panel to view list of downloads");
  let downloadsButton = document.getElementById("downloads-button");
  EventUtils.synthesizeMouseAtCenter(downloadsButton, {});
  let downloadsListBox = document.getElementById("downloadsListBox");

  ok(downloadsListBox, "downloadsListBox richlistitem should be visible");
  is(
    downloadsListBox.childElementCount,
    1,
    "downloadsListBox should have 1 download"
  );

  DownloadsPanel.hidePanel();
  await BrowserTestUtils.waitForCondition(
    () => !downloadsListBox.getAttribute("disabled")
  );
  info("downloadsListBox 'disabled' attribute should not exist");

  info("Cleaning up downloads");
  await task_resetState();
});
