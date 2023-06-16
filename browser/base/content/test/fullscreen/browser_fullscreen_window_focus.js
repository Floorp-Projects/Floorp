/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function pause() {
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  return new Promise(resolve => setTimeout(resolve, 500));
}

// This test tends to trigger a race in the fullscreen time telemetry,
// where the fullscreen enter and fullscreen exit events (which use the
// same histogram ID) overlap. That causes TelemetryStopwatch to log an
// error.
SimpleTest.ignoreAllUncaughtExceptions(true);

const IFRAME_ID = "testIframe";

async function testWindowFocus(isPopup, iframeID) {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  info("Calling window.open()");
  let openedWindow = await jsWindowOpen(tab.linkedBrowser, isPopup, iframeID);
  info("Letting OOP focus to stabilize");
  await pause(); // Bug 1719659 for proper fix
  info("re-focusing main window");
  await waitForFocus(tab.linkedBrowser);

  info("Entering full-screen");
  await changeFullscreen(tab.linkedBrowser, true);

  await testExpectFullScreenExit(
    tab.linkedBrowser,
    true,
    async () => {
      info("Calling window.focus()");
      await jsWindowFocus(tab.linkedBrowser, iframeID);
    },
    () => {
      // Async fullscreen transitions will swallow the repaint of the tab,
      // preventing us from detecting that we've successfully changed
      // fullscreen. Supply an action to switch back to the tab after the
      // fullscreen event has been received, which will ensure that the
      // tab is repainted when the DOMFullscreenChild is listening for it.
      info("Calling switchTab()");
      BrowserTestUtils.switchTab(gBrowser, tab);
    }
  );

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
  info("Letting OOP focus to stabilize");
  await pause(); // Bug 1719659 for proper fix
  info("re-focusing main window");
  await waitForFocus(tab.linkedBrowser);

  info("Entering full-screen");
  await changeFullscreen(tab.linkedBrowser, true);

  await testExpectFullScreenExit(
    tab.linkedBrowser,
    false,
    async () => {
      info("Calling element.focus() on popup");
      await ContentTask.spawn(tab.linkedBrowser, {}, async args => {
        await content.wrappedJSObject.sendMessage(
          content.wrappedJSObject.openedWindow,
          "elementfocus"
        );
      });
    },
    () => {
      // Async fullscreen transitions will swallow the repaint of the tab,
      // preventing us from detecting that we've successfully changed
      // fullscreen. Supply an action to switch back to the tab after the
      // fullscreen event has been received, which will ensure that the
      // tab is repainted when the DOMFullscreenChild is listening for it.
      info("Calling switchTab()");
      BrowserTestUtils.switchTab(gBrowser, tab);
    }
  );

  // Cleanup
  await changeFullscreen(tab.linkedBrowser, false);
  if (isPopup) {
    openedWindow.close();
  } else {
    BrowserTestUtils.removeTab(openedWindow);
  }
  BrowserTestUtils.removeTab(tab);
}

add_setup(async function () {
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
