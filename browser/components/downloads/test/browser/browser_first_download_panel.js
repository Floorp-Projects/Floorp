/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure the downloads panel only opens automatically on the first
 * download it notices. All subsequent downloads, even across sessions, should
 * not open the panel automatically.
 */
function gen_test()
{
  try {
    // Ensure that state is reset in case previous tests didn't finish.
    for (let yy in gen_resetState(DownloadsCommon.getData(window))) yield;

    // With this set to false, we should automatically open the panel
    // the first time a download is started.
    DownloadsCommon.getData(window).panelHasShownBefore = false;

    prepareForPanelOpen();
    DownloadsCommon.getData(window)._notifyNewDownload();
    yield;

    // If we got here, that means the panel opened.
    DownloadsPanel.hidePanel();

    ok(DownloadsCommon.getData(window).panelHasShownBefore,
       "Should have recorded that the panel was opened on a download.")

    // Next, make sure that if we start another download, we don't open
    // the panel automatically.
    panelShouldNotOpen();
    DownloadsCommon.getData(window)._notifyNewDownload();
    yield waitFor(2);
  } catch(e) {
    ok(false, e);
  } finally {
    // Clean up when the test finishes.
    for (let yy in gen_resetState(DownloadsCommon.getData(window))) yield;
  }
}

/**
 * Call this to record a test failure for the next time the downloads panel
 * opens.
 */
function panelShouldNotOpen()
{
  // Hook to wait until the test data has been loaded.
  let originalOnViewLoadCompleted = DownloadsPanel.onViewLoadCompleted;
  DownloadsPanel.onViewLoadCompleted = function () {
    DownloadsPanel.onViewLoadCompleted = originalOnViewLoadCompleted;
    ok(false, "Should not have opened the downloads panel.");
  };
}
