/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function waitOnTabSwitch() {
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(r => setTimeout(r, 300));
}

add_task(async function test_overlay_and_panel_state() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);

  let screenshotsTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PAGE
  );
  let browser = screenshotsTab.linkedBrowser;

  let helper = new ScreenshotsHelper(browser);

  helper.triggerUIFromToolbar();

  await helper.waitForOverlay();

  helper.assertPanelVisible();

  await helper.dragOverlay(10, 10, 500, 500);

  await helper.assertStateChange("selected");

  helper.assertPanelNotVisible();

  mouse.click(600, 600);

  await helper.assertStateChange("crosshairs");

  await helper.waitForOverlay();

  helper.assertPanelVisible();

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  await BrowserTestUtils.switchTab(gBrowser, screenshotsTab);

  await helper.waitForOverlayClosed();

  Assert.ok(!(await helper.isOverlayInitialized()), "Overlay is closed");
  helper.assertPanelNotVisible();

  helper.triggerUIFromToolbar();

  await helper.waitForOverlay();

  helper.assertPanelVisible();

  await helper.dragOverlay(10, 10, 500, 500);

  await helper.assertStateChange("selected");

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  await BrowserTestUtils.switchTab(gBrowser, screenshotsTab);

  Assert.ok(await helper.isOverlayInitialized(), "Overlay is open");
  helper.assertPanelNotVisible();

  helper.triggerUIFromToolbar();
  await helper.waitForOverlayClosed();

  Assert.ok(!(await helper.isOverlayInitialized()), "Overlay is closed");
  helper.assertPanelNotVisible();

  gBrowser.removeTab(tab1);
  gBrowser.removeTab(screenshotsTab);
});
