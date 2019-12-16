/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test tends to trigger a race in the fullscreen time telemetry,
// where the fullscreen enter and fullscreen exit events (which use the
// same histogram ID) overlap. That causes TelemetryStopwatch to log an
// error.
SimpleTest.ignoreAllUncaughtExceptions(true);

const TEST_URL =
  "http://example.com/browser/browser/base/content/test/fullscreen/open_and_focus_helper.html";
const IFRAME_ID = "testIframe";

async function testWindowFocus(iframeID) {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  info("Calling window.open()");
  let popup = await jsWindowOpen(tab.linkedBrowser, iframeID);
  info("re-focusing main window");
  await waitForFocus();

  info("Entering full-screen");
  await changeFullscreen(tab.linkedBrowser, true);

  await testExpectFullScreenExit(tab.linkedBrowser, true, async () => {
    info("Calling window.focus()");
    await jsWindowFocus(tab.linkedBrowser, iframeID);
  });

  // Cleanup
  popup.close();
  BrowserTestUtils.removeTab(tab);
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.disable_open_during_load", false], // Allow window.focus calls without user interaction
    ],
  });
});

add_task(function test_parentWindowFocus() {
  return testWindowFocus();
});

add_task(function test_iframeWindowFocus() {
  return testWindowFocus(IFRAME_ID);
});
