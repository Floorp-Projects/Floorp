/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure the downloads panel only opens automatically on the first
 * download it notices. All subsequent downloads, even across sessions, should
 * not open the panel automatically.
 */
function test_task()
{
  try {
    // Ensure that state is reset in case previous tests didn't finish.
    yield task_resetState();

    // With this set to false, we should automatically open the panel the first
    // time a download is started.
    DownloadsCommon.getData(window).panelHasShownBefore = false;

    let promise = promisePanelOpened();
    DownloadsCommon.getData(window)._notifyDownloadEvent("start");
    yield promise;

    // If we got here, that means the panel opened.
    DownloadsPanel.hidePanel();

    ok(DownloadsCommon.getData(window).panelHasShownBefore,
       "Should have recorded that the panel was opened on a download.")

    // Next, make sure that if we start another download, we don't open the
    // panel automatically.
    let originalOnPopupShown = DownloadsPanel.onPopupShown;
    DownloadsPanel.onPopupShown = function () {
      originalOnPopupShown.apply(this, arguments);
      ok(false, "Should not have opened the downloads panel.");
    };

    try {
      DownloadsCommon.getData(window)._notifyDownloadEvent("start");

      // Wait 2 seconds to ensure that the panel does not open.
      let deferTimeout = Promise.defer();
      setTimeout(deferTimeout.resolve, 2000);
      yield deferTimeout.promise;
    } finally {
      DownloadsPanel.onPopupShown = originalOnPopupShown;
    }
  } finally {
    // Clean up when the test finishes.
    yield task_resetState();
  }
}
