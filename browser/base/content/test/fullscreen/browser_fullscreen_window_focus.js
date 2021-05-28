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

async function testWindowFocus(isPopup, iframeID) {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  info("Calling window.open()");
  let openedWindow = await jsWindowOpen(tab.linkedBrowser, isPopup, iframeID);
  info("re-focusing main window");
  await waitForFocus(tab.linkedBrowser);

  info("Entering full-screen");
  await changeFullscreen(tab.linkedBrowser, true);

  await testExpectFullScreenExit(tab.linkedBrowser, true, async () => {
    info("Calling window.focus()");
    await jsWindowFocus(tab.linkedBrowser, iframeID);
  });

  // Cleanup
  if (isPopup) {
    openedWindow.close();
  } else {
    BrowserTestUtils.removeTab(openedWindow);
  }
  BrowserTestUtils.removeTab(tab);
}

async function testWindowElementFocus(isPopup) {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  info("Calling window.open()");
  let openedWindow = await jsWindowOpen(tab.linkedBrowser, isPopup);
  info("re-focusing main window");
  await waitForFocus(tab.linkedBrowser);

  info("Entering full-screen");
  await changeFullscreen(tab.linkedBrowser, true);

  await testExpectFullScreenExit(tab.linkedBrowser, false, async () => {
    info("Calling element.focus() on popup");
    await ContentTask.spawn(tab.linkedBrowser, {}, async args => {
      await content.wrappedJSObject.sendMessage(
        content.wrappedJSObject.openedWindow,
        "elementfocus"
      );
    });
  });

  // Cleanup
  await changeFullscreen(tab.linkedBrowser, false);
  if (isPopup) {
    openedWindow.close();
  } else {
    BrowserTestUtils.removeTab(openedWindow);
  }
  BrowserTestUtils.removeTab(tab);
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.disable_open_during_load", false], // Allow window.focus calls without user interaction
      ["browser.link.open_newwindow.disabled_in_fullscreen", false],
    ],
  });
});

add_task(function test_popupWindowFocus() {
  return testWindowFocus(true);
});

add_task(function test_iframePopupWindowFocus() {
  return testWindowFocus(true, IFRAME_ID);
});

add_task(function test_popupWindowElementFocus() {
  return testWindowElementFocus(true);
});

add_task(function test_backgroundTabFocus() {
  return testWindowFocus(false);
});

add_task(function test_iframebackgroundTabFocus() {
  return testWindowFocus(false, IFRAME_ID);
});

add_task(function test_backgroundTabElementFocus() {
  return testWindowElementFocus(false);
});
