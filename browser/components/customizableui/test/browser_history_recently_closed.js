/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

/**
 * Opens the history panel through the history toolbarbutton in the
 * navbar and returns a promise that resolves as soon as the panel is open
 * is showing.
 */
async function openHistoryPanel() {
  await waitForOverflowButtonShown();
  await document.getElementById("nav-bar").overflowable.show();
  info("Menu panel was opened");

  let historyButton = document.getElementById("history-panelmenu");
  Assert.ok(historyButton, "History button appears in Panel Menu");

  historyButton.click();

  let historyPanel = document.getElementById("PanelUI-history");
  return BrowserTestUtils.waitForEvent(historyPanel, "ViewShown");
}

/**
 * Closes the history panel and returns a promise that resolves as sooon
 * as the panel is closed.
 */
async function hideHistoryPanel() {
  let historyView = document.getElementById("PanelUI-history");
  let historyPanel = historyView.closest("panel");
  let promise = BrowserTestUtils.waitForEvent(historyPanel, "popuphidden");
  historyPanel.hidePopup();
  return promise;
}

add_task(async function testRecentlyClosedDisabled() {
  info("Check history recently closed tabs/windows section");

  CustomizableUI.addWidgetToArea(
    "history-panelmenu",
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
  );
  registerCleanupFunction(() => CustomizableUI.reset());

  // We need to make sure the history is cleared before starting the test
  await Sanitizer.sanitize(["history"]);

  await openHistoryPanel();

  let recentlyClosedTabs = document.getElementById("appMenuRecentlyClosedTabs");
  let recentlyClosedWindows = document.getElementById(
    "appMenuRecentlyClosedWindows"
  );

  // Wait for the disabled attribute to change, as we receive
  // the "viewshown" event before this changes
  await BrowserTestUtils.waitForCondition(
    () => recentlyClosedTabs.getAttribute("disabled"),
    "Waiting for button to become disabled"
  );
  Assert.ok(
    recentlyClosedTabs.getAttribute("disabled"),
    "Recently closed tabs button disabled"
  );
  Assert.ok(
    recentlyClosedWindows.getAttribute("disabled"),
    "Recently closed windows button disabled"
  );

  await hideHistoryPanel();

  gBrowser.selectedTab.focus();
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PATH + "dummy_history_item.html"
  );
  BrowserTestUtils.removeTab(tab);

  await openHistoryPanel();

  await BrowserTestUtils.waitForCondition(
    () => !recentlyClosedTabs.getAttribute("disabled"),
    "Waiting for button to be enabled"
  );
  Assert.ok(
    !recentlyClosedTabs.getAttribute("disabled"),
    "Recently closed tabs is available"
  );
  Assert.ok(
    recentlyClosedWindows.getAttribute("disabled"),
    "Recently closed windows button disabled"
  );

  await hideHistoryPanel();

  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let loadedPromise = BrowserTestUtils.browserLoaded(
    newWin.gBrowser.selectedBrowser
  );
  BrowserTestUtils.loadURI(newWin.gBrowser.selectedBrowser, "about:mozilla");
  await loadedPromise;
  await BrowserTestUtils.closeWindow(newWin);

  await openHistoryPanel();

  await BrowserTestUtils.waitForCondition(
    () => !recentlyClosedWindows.getAttribute("disabled"),
    "Waiting for button to be enabled"
  );
  Assert.ok(
    !recentlyClosedTabs.getAttribute("disabled"),
    "Recently closed tabs is available"
  );
  Assert.ok(
    !recentlyClosedWindows.getAttribute("disabled"),
    "Recently closed windows is available"
  );

  await hideHistoryPanel();
});
