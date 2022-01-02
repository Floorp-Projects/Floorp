/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

/**
 * Make sure the downloads panel only opens automatically on the first
 * download it notices. All subsequent downloads, even across sessions, should
 * not open the panel automatically.
 */
add_task(async function test_first_download_panel() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.autohideButton", false]],
  });
  await promiseButtonShown("downloads-button");
  // Clear the download panel has shown preference first as this test is used to
  // verify this preference's behaviour.
  let oldPrefValue = Services.prefs.getBoolPref("browser.download.panel.shown");
  Services.prefs.setBoolPref("browser.download.panel.shown", false);

  registerCleanupFunction(async function() {
    // Clean up when the test finishes.
    await task_resetState();

    // Set the preference instead of clearing it afterwards to ensure the
    // right value is used no matter what the default was. This ensures the
    // panel doesn't appear and affect other tests.
    Services.prefs.setBoolPref("browser.download.panel.shown", oldPrefValue);
  });

  // Ensure that state is reset in case previous tests didn't finish.
  await task_resetState();

  // With this set to false, we should automatically open the panel the first
  // time a download is started.
  DownloadsCommon.getData(window).panelHasShownBefore = false;

  info("waiting for panel open");
  let promise = promisePanelOpened();
  DownloadsCommon.getData(window)._notifyDownloadEvent("start");
  await promise;

  // If we got here, that means the panel opened.
  DownloadsPanel.hidePanel();

  ok(
    DownloadsCommon.getData(window).panelHasShownBefore,
    "Should have recorded that the panel was opened on a download."
  );

  // If browser.download.improvements_to_download_panel is enabled, this will fail
  // because we always open a downloads panel as long as there is no other simulatenous
  // download. So, first ensure that this pref is already false.
  if (
    !SpecialPowers.getBoolPref(
      "browser.download.improvements_to_download_panel"
    )
  ) {
    // Next, make sure that if we start another download, we don't open the
    // panel automatically.
    let originalOnPopupShown = DownloadsPanel.onPopupShown;
    DownloadsPanel.onPopupShown = function() {
      originalOnPopupShown.apply(this, arguments);
      ok(false, "Should not have opened the downloads panel.");
    };

    DownloadsCommon.getData(window)._notifyDownloadEvent("start");

    // Wait 2 seconds to ensure that the panel does not open.
    await new Promise(resolve => setTimeout(resolve, 2000));
    DownloadsPanel.onPopupShown = originalOnPopupShown;
  }
});
