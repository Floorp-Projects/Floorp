/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure the downloads panel only opens automatically on the first
 * download it notices. All subsequent downloads, even across sessions, should
 * not open the panel automatically.
 */
add_task(function* test_first_download_panel() {
  // Clear the download panel has shown preference first as this test is used to
  // verify this preference's behaviour.
  let oldPrefValue = Services.prefs.getBoolPref("browser.download.panel.shown");
  Services.prefs.setBoolPref("browser.download.panel.shown", false);

  registerCleanupFunction(function*() {
    // Clean up when the test finishes.
    yield task_resetState();

    // Set the preference instead of clearing it afterwards to ensure the
    // right value is used no matter what the default was. This ensures the
    // panel doesn't appear and affect other tests.
    Services.prefs.setBoolPref("browser.download.panel.shown", oldPrefValue);
  });

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

  DownloadsCommon.getData(window)._notifyDownloadEvent("start");

  // Wait 2 seconds to ensure that the panel does not open.
  yield new Promise(resolve => setTimeout(resolve, 2000));
  DownloadsPanel.onPopupShown = originalOnPopupShown;
});
