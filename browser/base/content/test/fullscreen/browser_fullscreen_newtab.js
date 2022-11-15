/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test verifies that when in fullscreen mode, and a new tab is opened,
// fullscreen mode is exited and the url bar is focused.
add_task(async function test_fullscreen_display_none() {
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

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    content.document.getElementById("request").click();
  });

  await fullScreenEntered;

  let fullScreenExited = BrowserTestUtils.waitForEvent(
    document,
    "fullscreenchange",
    false,
    () => !document.fullscreenElement
  );

  let focusPromise = BrowserTestUtils.waitForEvent(window, "focus");
  EventUtils.synthesizeKey("T", { accelKey: true });
  await focusPromise;

  is(
    document.activeElement,
    gURLBar.inputField,
    "url bar is focused after new tab opened"
  );

  await fullScreenExited;

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  BrowserTestUtils.removeTab(tab);
});
