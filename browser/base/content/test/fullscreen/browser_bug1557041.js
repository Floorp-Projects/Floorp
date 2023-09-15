/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test tends to trigger a race in the fullscreen time telemetry,
// where the fullscreen enter and fullscreen exit events (which use the
// same histogram ID) overlap. That causes TelemetryStopwatch to log an
// error.
SimpleTest.ignoreAllUncaughtExceptions(true);

add_task(async function test_identityPopupCausesFSExit() {
  let url = "https://example.com/";

  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    let loaded = BrowserTestUtils.browserLoaded(browser, false, url);
    BrowserTestUtils.startLoadingURIString(browser, url);
    await loaded;

    let identityPermissionBox = document.getElementById(
      "identity-permission-box"
    );

    info("Entering DOM fullscreen");
    await changeFullscreen(browser, true);

    let popupShown = BrowserTestUtils.waitForEvent(
      window,
      "popupshown",
      true,
      event => event.target == document.getElementById("permission-popup")
    );
    let fsExit = waitForFullScreenState(browser, false);

    identityPermissionBox.click();

    info("Waiting for fullscreen exit and permission popup to show");
    await Promise.all([fsExit, popupShown]);

    let identityPopup = document.getElementById("permission-popup");
    ok(
      identityPopup.hasAttribute("panelopen"),
      "Identity popup should be open"
    );
    ok(!window.fullScreen, "Should not be in full-screen");
  });
});
