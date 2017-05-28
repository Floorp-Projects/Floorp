"use strict";

function isFullscreenSizeMode() {
  let sizemode = document.documentElement.getAttribute("sizemode");
  return sizemode == "fullscreen";
}

/**
 * Checks that the URL bar is correctly focused
 * when a new tab is opened while in fullscreen
 * mode.
 */
add_task(async function() {
  gURLBar.blur();

  Assert.ok(!window.fullScreen, "Should not start in fullscreen mode.");
  BrowserFullScreen();
  await BrowserTestUtils.waitForCondition(() => isFullscreenSizeMode());

  registerCleanupFunction(async function() {
    // Exit fullscreen if we're still in it.
    if (window.fullScreen) {
      BrowserFullScreen();
      await BrowserTestUtils.waitForCondition(() => !isFullscreenSizeMode());
    }
  });

  Assert.ok(window.fullScreen, "Should be in fullscreen mode now.");

  let newTabOpened = BrowserTestUtils.waitForEvent(gBrowser, "TabSwitchDone");
  BrowserOpenTab();
  await newTabOpened;

  Assert.ok(gURLBar.focused, "URL bar should be focused.");
});
