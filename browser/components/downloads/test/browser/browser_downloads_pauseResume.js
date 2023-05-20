/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

registerCleanupFunction(async function () {
  await task_resetState();
});

add_task(async function test_downloads_library() {
  let DownloadData = [];
  for (let i = 0; i < 20; i++) {
    DownloadData.push({ state: DownloadsCommon.DOWNLOAD_PAUSED });
  }

  // Ensure that state is reset in case previous tests didn't finish.
  await task_resetState();

  // Populate the downloads database with the data required by this test.
  await task_addDownloads(DownloadData);

  let win = await openLibrary("Downloads");
  registerCleanupFunction(function () {
    win.close();
  });

  let listbox = win.document.getElementById("downloadsListBox");
  ok(listbox, "Download list box present");

  // Select one of the downloads.
  listbox.itemChildren[0].click();
  listbox.itemChildren[0]._shell._download.hasPartialData = true;

  EventUtils.synthesizeKey(" ", {}, win);
  is(
    listbox.itemChildren[0]._shell._downloadState,
    DownloadsCommon.DOWNLOAD_DOWNLOADING,
    "Download state toggled from paused to downloading"
  );

  // there is no event to wait for in some cases, we need to wait for the keypress to potentially propagate
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(r => setTimeout(r, 500));
  is(
    listbox.scrollTop,
    0,
    "All downloads view did not scroll when spacebar event fired on a selected download"
  );
});
