/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

registerCleanupFunction(function*() {
  yield task_resetState();
});

/**
 * Make sure the downloads panel can display items in the right order and
 * contains the expected data.
 */
add_task(function* test_basic_functionality() {
  // Display one of each download state.
  const DownloadData = [
    { state: nsIDM.DOWNLOAD_NOTSTARTED },
    { state: nsIDM.DOWNLOAD_PAUSED },
    { state: nsIDM.DOWNLOAD_FINISHED },
    { state: nsIDM.DOWNLOAD_FAILED },
    { state: nsIDM.DOWNLOAD_CANCELED },
  ];

  // Wait for focus first
  yield promiseFocus();

  // Ensure that state is reset in case previous tests didn't finish.
  yield task_resetState();

  // For testing purposes, show all the download items at once.
  var originalCountLimit = DownloadsView.kItemCountLimit;
  DownloadsView.kItemCountLimit = DownloadData.length;
  registerCleanupFunction(function () {
    DownloadsView.kItemCountLimit = originalCountLimit;
  });

  // Populate the downloads database with the data required by this test.
  yield task_addDownloads(DownloadData);

  // Open the user interface and wait for data to be fully loaded.
  yield task_openPanel();

  // Test item data and count.  This also tests the ordering of the display.
  let richlistbox = document.getElementById("downloadsListBox");
  /* disabled for failing intermittently (bug 767828)
    is(richlistbox.children.length, DownloadData.length,
       "There is the correct number of richlistitems");
  */
  let itemCount = richlistbox.children.length;
  for (let i = 0; i < itemCount; i++) {
    let element = richlistbox.children[itemCount - i - 1];
    let download = DownloadsView.controllerForElement(element).download;
    is(DownloadsCommon.stateOfDownload(download), DownloadData[i].state,
       "Download states match up");
  }
});
