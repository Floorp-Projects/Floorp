/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test tends to trigger a race in the fullscreen time telemetry,
// where the fullscreen enter and fullscreen exit events (which use the
// same histogram ID) overlap. That causes TelemetryStopwatch to log an
// error.
SimpleTest.ignoreAllUncaughtExceptions(true);
SimpleTest.requestLongerTimeout(2);

const IFRAME_ID = "testIframe";

async function testWindowOpen(iframeID) {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  info("Entering full-screen");
  await changeFullscreen(tab.linkedBrowser, true);

  let popup;
  await testExpectFullScreenExit(tab.linkedBrowser, true, async () => {
    info("Calling window.open()");
    popup = await jsWindowOpen(tab.linkedBrowser, true, iframeID);
  });

  // Cleanup
  await BrowserTestUtils.closeWindow(popup);
  BrowserTestUtils.removeTab(tab);
}

async function testWindowOpenExistingWindow() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  let popup = await jsWindowOpen(tab.linkedBrowser, true);

  info("re-focusing main window");
  await waitForFocus(tab.linkedBrowser);

  info("Entering full-screen");
  await changeFullscreen(tab.linkedBrowser, true);

  await testExpectFullScreenExit(tab.linkedBrowser, true, async () => {
    info("Calling window.open() again should reuse the existing window");
    jsWindowOpen(tab.linkedBrowser, true);
  });

  // Cleanup
  await BrowserTestUtils.closeWindow(popup);
  BrowserTestUtils.removeTab(tab);
}

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.disable_open_during_load", false], // Allow window.open calls without user interaction
      ["browser.link.open_newwindow.disabled_in_fullscreen", false],
    ],
  });
});

add_task(function test_parentWindowOpen() {
  return testWindowOpen();
});

add_task(function test_iframeWindowOpen() {
  return testWindowOpen(IFRAME_ID);
});

add_task(function test_parentWindowOpenExistWindow() {
  return testWindowOpenExistingWindow();
});
