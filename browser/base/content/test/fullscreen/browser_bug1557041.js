/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { BrowserTestUtils } = ChromeUtils.import(
  "resource://testing-common/BrowserTestUtils.jsm"
);

// This test tends to trigger a race in the fullscreen time telemetry,
// where the fullscreen enter and fullscreen exit events (which use the
// same histogram ID) overlap. That causes TelemetryStopwatch to log an
// error.
SimpleTest.ignoreAllUncaughtExceptions(true);

add_task(async function test_identityPopupCausesFSExit() {
  let url = "https://example.com/";

  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    let loaded = BrowserTestUtils.browserLoaded(browser, false, url);
    BrowserTestUtils.loadURI(browser, url);
    await loaded;

    let identityBox = document.getElementById("identity-box");
    let identityPopup = document.getElementById("identity-popup");

    info("Entering DOM fullscreen");
    await changeFullscreen(browser, true);

    let popupShown = BrowserTestUtils.waitForEvent(
      identityPopup,
      "popupshown",
      true
    );
    let fsExit = waitForFullScreenState(browser, false);

    identityBox.click();

    info("Waiting for fullscreen exit and identity popup to show");
    await Promise.all([fsExit, popupShown]);

    ok(
      identityPopup.hasAttribute("panelopen"),
      "Identity popup should be open"
    );
    ok(!window.fullScreen, "Should not be in full-screen");
  });
});
