/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.always_ask_before_handling_new_types", false]],
  });

  registerCleanupFunction(async () => {
    info("Resetting downloads and closing downloads panel");
    await task_resetState();
  });

  let downloadList = await Downloads.getList(Downloads.ALL);
  let downloadCount = (await downloadList.getAll()).length;
  is(downloadCount, 0, "At the start of the test, there should be 0 downloads");
});

// Test that the top item in the panel always gets focus upon opening the panel.
add_task(async function test_focus() {
  info("creating a download and setting it to in progress");
  await task_addDownloads([{ state: DownloadsCommon.DOWNLOAD_DOWNLOADING }]);
  let publicList = await Downloads.getList(Downloads.PUBLIC);
  let downloads = await publicList.getAll();
  downloads[0].stopped = false;

  info("waiting for the panel to open");
  await task_openPanel();
  await BrowserTestUtils.waitForCondition(
    () => !DownloadsView.richListBox.getAttribute("disabled")
  );

  is(
    DownloadsView.richListBox.itemCount,
    1,
    "there should be exactly one download listed"
  );
  // Most of the time if we want to check which thing has focus, we can just ask
  // Services.focus to tell us. But the downloads panel uses a <richlistbox>,
  // and when an item in one of those has focus, the focus manager actually
  // thinks that *the list itself* has focus, and everything below that is
  // handled within the widget. So, the best we can do is check that the list is
  // focused and then that the selected item within the list is correct.
  is(
    Services.focus.focusedElement,
    DownloadsView.richListBox,
    "the downloads list should have focus"
  );
  is(
    DownloadsView.richListBox.itemChildren[0],
    DownloadsView.richListBox.selectedItem,
    "the focused item should be the only download in the list"
  );

  info("closing the panel and creating a second download");
  DownloadsPanel.hidePanel();
  await task_addDownloads([{ state: DownloadsCommon.DOWNLOAD_DOWNLOADING }]);

  info("waiting for the panel to open after starting the second download");
  await task_openPanel();
  await BrowserTestUtils.waitForCondition(
    () => !DownloadsView.richListBox.getAttribute("disabled")
  );

  is(
    DownloadsView.richListBox.itemCount,
    2,
    "there should be two downloads listed"
  );
  is(
    Services.focus.focusedElement,
    DownloadsView.richListBox,
    "the downloads list should have focus"
  );
  is(
    DownloadsView.richListBox.itemChildren[0],
    DownloadsView.richListBox.selectedItem,
    "the focused item should be the first download in the list"
  );

  info("closing the panel and creating a third download");
  DownloadsPanel.hidePanel();
  await task_addDownloads([{ state: DownloadsCommon.DOWNLOAD_DOWNLOADING }]);

  info("waiting for the panel to open after starting the third download");
  await task_openPanel();
  await BrowserTestUtils.waitForCondition(
    () => !DownloadsView.richListBox.getAttribute("disabled")
  );

  is(
    DownloadsView.richListBox.itemCount,
    3,
    "there should be three downloads listed"
  );
  is(
    Services.focus.focusedElement,
    DownloadsView.richListBox,
    "the downloads list should have focus"
  );
  is(
    DownloadsView.richListBox.itemChildren[0],
    DownloadsView.richListBox.selectedItem,
    "the focused item should be the first download in the list"
  );
});
