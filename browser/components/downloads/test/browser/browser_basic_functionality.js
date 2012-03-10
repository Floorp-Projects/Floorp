/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure the downloads panel can display items in the right order and
 * contains the expected data.
 */
function gen_test()
{
  // Display one of each download state.
  const DownloadData = [
    { endTime: 1180493839859239, state: nsIDM.DOWNLOAD_NOTSTARTED },
    { endTime: 1180493839859238, state: nsIDM.DOWNLOAD_DOWNLOADING },
    { endTime: 1180493839859237, state: nsIDM.DOWNLOAD_PAUSED },
    { endTime: 1180493839859236, state: nsIDM.DOWNLOAD_SCANNING },
    { endTime: 1180493839859235, state: nsIDM.DOWNLOAD_QUEUED },
    { endTime: 1180493839859234, state: nsIDM.DOWNLOAD_FINISHED },
    { endTime: 1180493839859233, state: nsIDM.DOWNLOAD_FAILED },
    { endTime: 1180493839859232, state: nsIDM.DOWNLOAD_CANCELED },
    { endTime: 1180493839859231, state: nsIDM.DOWNLOAD_BLOCKED_PARENTAL },
    { endTime: 1180493839859230, state: nsIDM.DOWNLOAD_DIRTY },
    { endTime: 1180493839859229, state: nsIDM.DOWNLOAD_BLOCKED_POLICY },
  ];

  try {
    // Ensure that state is reset in case previous tests didn't finish.
    for (let yy in gen_resetState()) yield;

    // Populate the downloads database with the data required by this test.
    for (let yy in gen_addDownloadRows(DownloadData)) yield;

    // Open the user interface and wait for data to be fully loaded.
    for (let yy in gen_openPanel()) yield;

    // Test item data and count.  This also tests the ordering of the display.
    let richlistbox = document.getElementById("downloadsListBox");
    is(richlistbox.children.length, DownloadData.length,
       "There is the correct number of richlistitems");
    for (let i = 0; i < richlistbox.children.length; i++) {
      let element = richlistbox.children[i];
      let dataItem = new DownloadsViewItemController(element).dataItem;
      is(dataItem.target, DownloadData[i].name, "Download names match up");
      is(dataItem.state, DownloadData[i].state, "Download states match up");
      is(dataItem.file, DownloadData[i].target, "Download targets match up");
      is(dataItem.uri, DownloadData[i].source, "Download sources match up");
    }
  } finally {
    // Clean up when the test finishes.
    for (let yy in gen_resetState()) yield;
  }
}
