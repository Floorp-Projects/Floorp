/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test verifies that when in fullscreen mode, and a new window is opened,
// fullscreen mode should not exit and the url bar is focused.
add_task(async function test_fullscreen_new_window() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["full-screen-api.enabled", true],
      ["full-screen-api.allow-trusted-requests-only", false],
    ],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.org/browser/browser/base/content/test/fullscreen/fullscreen.html"
  );

  let fullScreenEntered = BrowserTestUtils.waitForEvent(
    document,
    "fullscreenchange",
    false,
    () => document.fullscreenElement
  );

  // Enter fullscreen.
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    content.document.getElementById("request").click();
  });

  await fullScreenEntered;

  // Open a new window via ctrl+n.
  let newWindowPromise = BrowserTestUtils.waitForNewWindow({
    url: "about:blank",
  });
  EventUtils.synthesizeKey("N", { accelKey: true });
  let newWindow = await newWindowPromise;

  // Check new window state.
  is(
    newWindow.document.activeElement,
    newWindow.gURLBar.inputField,
    "url bar is focused after new window opened"
  );
  ok(
    !newWindow.fullScreen,
    "The new chrome window should not be in fullscreen"
  );
  ok(
    !newWindow.document.documentElement.hasAttribute("inDOMFullscreen"),
    "The new chrome document should not be in fullscreen"
  );

  // Wait a bit then check the original window state.
  await new Promise(resolve => TestUtils.executeSoon(resolve));
  ok(
    window.fullScreen,
    "The original chrome window should be still in fullscreen"
  );
  ok(
    document.documentElement.hasAttribute("inDOMFullscreen"),
    "The original chrome document should be still in fullscreen"
  );

  // Close new window and move focus back to original window.
  await BrowserTestUtils.closeWindow(newWindow);
  await SimpleTest.promiseFocus(window);

  // Exit fullscreen on original window.
  let fullScreenExited = BrowserTestUtils.waitForEvent(
    document,
    "fullscreenchange",
    false,
    () => !document.fullscreenElement
  );
  EventUtils.synthesizeKey("KEY_Escape");
  await fullScreenExited;

  BrowserTestUtils.removeTab(tab);
});
