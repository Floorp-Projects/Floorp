/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

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

add_task(async function testRecentlyClosedTabsDisabledPersists() {
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
  Assert.ok(
    recentlyClosedTabs.getAttribute("disabled"),
    "Recently closed tabs button disabled"
  );

  await hideHistoryPanel();

  let newWin = await BrowserTestUtils.openNewBrowserWindow();

  await openHistoryPanel(newWin.document);
  recentlyClosedTabs = newWin.document.getElementById(
    "appMenuRecentlyClosedTabs"
  );
  Assert.ok(
    recentlyClosedTabs.getAttribute("disabled"),
    "Recently closed tabs is disabled"
  );

  // We close the window without hiding the panel first, which used to interfere
  // with populating the view subsequently.
  await BrowserTestUtils.closeWindow(newWin);

  newWin = await BrowserTestUtils.openNewBrowserWindow();
  await openHistoryPanel(newWin.document);
  recentlyClosedTabs = newWin.document.getElementById(
    "appMenuRecentlyClosedTabs"
  );
  Assert.ok(
    recentlyClosedTabs.getAttribute("disabled"),
    "Recently closed tabs is disabled"
  );
  await hideHistoryPanel(newWin.document);
  await BrowserTestUtils.closeWindow(newWin);
});
