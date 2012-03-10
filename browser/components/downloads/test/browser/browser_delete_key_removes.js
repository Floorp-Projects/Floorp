/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test ensures that the delete key removes a download.
 */
function gen_test()
{
  const DownloadData = [
    { state: nsIDM.DOWNLOAD_FINISHED },
    { state: nsIDM.DOWNLOAD_FAILED },
    { state: nsIDM.DOWNLOAD_CANCELED },
    { state: nsIDM.DOWNLOAD_BLOCKED_PARENTAL },
    { state: nsIDM.DOWNLOAD_DIRTY },
    { state: nsIDM.DOWNLOAD_BLOCKED_POLICY },
  ];

  try {
    // Ensure that state is reset in case previous tests didn't finish.
    for (let yy in gen_resetState()) yield;

    // Populate the downloads database with the data required by this test.
    for (let yy in gen_addDownloadRows(DownloadData)) yield;

    // Open the user interface and wait for data to be fully loaded.
    for (let yy in gen_openPanel()) yield;

    // Test if items are deleted properly.
    let richlistbox = document.getElementById("downloadsListBox");
    let statement = Services.downloads.DBConnection.createAsyncStatement(
                    "SELECT COUNT(*) FROM moz_downloads");
    try {
      // Check the number of downloads initially present.
      statement.executeAsync({
        handleResult: function(aResultSet)
        {
          is(aResultSet.getNextRow().getResultByIndex(0),
             richlistbox.children.length,
             "The database and the number of downloads display matches");
        },
        handleError: function(aError)
        {
          Cu.reportError(aError);
        },
        handleCompletion: function(aReason)
        {
          testRunner.continueTest();
        }
      });
      yield;

      // Check that the delete key removes each download.
      let len = DownloadData.length;
      for (let i = 0; i < len; i++) {
        // Wait for focus on the main window.
        waitForFocus(testRunner.continueTest);
        yield;

        // This removes the download synchronously.
        EventUtils.synthesizeKey("VK_DELETE", {});

        // Run the statement asynchronously and wait.
        statement.executeAsync({
          handleResult: function(aResultSet)
          {
            is(aResultSet.getNextRow().getResultByIndex(0),
               len - (i + 1),
               "The download was properly removed");
          },
          handleError: function(aError)
          {
            Cu.reportError(aError);
          },
          handleCompletion: function(aReason)
          {
            testRunner.continueTest();
          }
        });
        yield;
      }
    } finally {
      statement.finalize();
    }
  } finally {
    // Clean up when the test finishes.
    for (let yy in gen_resetState()) yield;
  }
}
