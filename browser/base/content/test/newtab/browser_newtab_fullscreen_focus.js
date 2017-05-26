"use strict";

/**
 * Checks that the URL bar is correctly focused
 * when a new tab is opened while in fullscreen
 * mode.
 */
add_task(async function() {
  gURLBar.blur();

  Assert.ok(!window.fullScreen, "Should not start in fullscreen mode.");

  let enterFs = BrowserTestUtils.waitForEvent(window, "fullscreen");
  BrowserFullScreen();
  await enterFs;

  registerCleanupFunction(async function() {
    // Exit fullscreen if we're still in it.
    if (window.fullScreen) {
      let exitFs = BrowserTestUtils.waitForEvent(window, "fullscreen");
      BrowserFullScreen();
      await exitFs;
    }
  });

  Assert.ok(window.fullScreen, "Should be in fullscreen mode now.");

  let newTabOpened = BrowserTestUtils.waitForEvent(gBrowser, "TabSwitchDone");
  BrowserOpenTab();
  await newTabOpened;

  Assert.ok(gURLBar.focused, "URL bar should be focused.");
});
