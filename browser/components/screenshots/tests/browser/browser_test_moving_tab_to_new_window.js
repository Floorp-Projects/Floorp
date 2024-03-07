/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_movingTabToNewWindow() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);
  let originalHelper = new ScreenshotsHelper(tab.linkedBrowser);
  originalHelper.triggerUIFromToolbar();
  await originalHelper.waitForOverlay();
  await originalHelper.dragOverlay(10, 10, 300, 300);

  let newWindow = gBrowser.replaceTabWithWindow(tab);
  let swapDocShellPromise = BrowserTestUtils.waitForEvent(
    tab.linkedBrowser,
    "SwapDocShells"
  );
  await swapDocShellPromise;

  let newtab = newWindow.gBrowser.selectedTab;
  let newHelper = new ScreenshotsHelper(newtab.linkedBrowser);

  let isInitialized = await newHelper.isOverlayInitialized();

  ok(isInitialized, "Overlay is initialized after switching windows");
  ok(
    !ScreenshotsUtils.browserToScreenshotsState.has(tab.linkedBrowser),
    "The old browser is no longer in the ScreenshotsUtils weakmap"
  );
  ok(
    ScreenshotsUtils.browserToScreenshotsState.has(newtab.linkedBrowser),
    "The new browser is in the ScreenshotsUtils weakmap"
  );

  await newHelper.clickCancelButton();
  await newHelper.assertStateChange("crosshairs");
  await newHelper.waitForOverlay();

  swapDocShellPromise = BrowserTestUtils.waitForEvent(
    newtab.linkedBrowser,
    "SwapDocShells"
  );
  gBrowser.adoptTab(newWindow.gBrowser.selectedTab, 1, true);
  await swapDocShellPromise;

  tab = gBrowser.selectedTab;
  let helper = new ScreenshotsHelper(tab.linkedBrowser);

  isInitialized = await helper.isOverlayInitialized();

  ok(!isInitialized, "Overlay is not initialized");
  ok(
    !ScreenshotsUtils.browserToScreenshotsState.has(tab.linkedBrowser),
    "The old browser is no longer in the ScreenshotsUtils weakmap"
  );
  ok(
    !ScreenshotsUtils.browserToScreenshotsState.has(newtab.linkedBrowser),
    "The new browser is no longer in the ScreenshotsUtils weakmap"
  );

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_movingParentProcessTabToNewWindow() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:robots"
  );
  let originalHelper = new ScreenshotsHelper(tab.linkedBrowser);
  originalHelper.triggerUIFromToolbar();
  await originalHelper.waitForOverlay();
  await originalHelper.dragOverlay(10, 10, 300, 300);

  let newWindow = gBrowser.replaceTabWithWindow(tab);
  let swapDocShellPromise = BrowserTestUtils.waitForEvent(
    tab.linkedBrowser,
    "SwapDocShells"
  );
  await swapDocShellPromise;

  let newtab = newWindow.gBrowser.selectedTab;
  let newHelper = new ScreenshotsHelper(newtab.linkedBrowser);

  let isInitialized = await newHelper.isOverlayInitialized();

  ok(isInitialized, "Overlay is initialized after switching windows");
  ok(
    !ScreenshotsUtils.browserToScreenshotsState.has(tab.linkedBrowser),
    "The old browser is no longer in the ScreenshotsUtils weakmap"
  );
  ok(
    ScreenshotsUtils.browserToScreenshotsState.has(newtab.linkedBrowser),
    "The new browser is in the ScreenshotsUtils weakmap"
  );

  await newHelper.clickCancelButton();
  await newHelper.assertStateChange("crosshairs");
  await newHelper.waitForOverlay();

  swapDocShellPromise = BrowserTestUtils.waitForEvent(
    newtab.linkedBrowser,
    "SwapDocShells"
  );
  gBrowser.adoptTab(newWindow.gBrowser.selectedTab, 1, true);
  await swapDocShellPromise;

  tab = gBrowser.selectedTab;
  let helper = new ScreenshotsHelper(tab.linkedBrowser);

  isInitialized = await helper.isOverlayInitialized();

  ok(!isInitialized, "Overlay is not initialized");
  ok(
    !ScreenshotsUtils.browserToScreenshotsState.has(tab.linkedBrowser),
    "The old browser is no longer in the ScreenshotsUtils weakmap"
  );
  ok(
    !ScreenshotsUtils.browserToScreenshotsState.has(newtab.linkedBrowser),
    "The new browser is no longer in the ScreenshotsUtils weakmap"
  );

  await BrowserTestUtils.removeTab(tab);
});
