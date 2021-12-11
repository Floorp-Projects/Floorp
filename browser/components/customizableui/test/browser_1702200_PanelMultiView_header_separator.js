/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests whether the separator insertion works correctly for the
 * special case where we remove the content except the header itself
 * before showing the panel view.
 */

const TEST_SITE = "http://127.0.0.1";
const RECENTLY_CLOSED_TABS_PANEL_ID = "appMenu-library-recentlyClosedTabs";
const RECENTLY_CLOSED_TABS_ITEM_ID = "appMenuRecentlyClosedTabs";

function assertHeaderSeparator() {
  let header = document.querySelector(
    `#${RECENTLY_CLOSED_TABS_PANEL_ID} .panel-header`
  );
  Assert.equal(
    header.nextSibling.tagName,
    "toolbarseparator",
    "toolbarseparator should be shown below header"
  );
}

/**
 * Open and close a tab so we can access the "Recently
 * closed tabs" panel
 */
add_task(async function test_setup() {
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_SITE);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser, false, null, true);
  await BrowserTestUtils.removeTab(tab);
});

/**
 * Tests whether the toolbarseparator is shown correctly
 * after re-entering same sub view, see bug 1702200
 *
 * - App Menu
 *   - History
 *     - Recently closed tabs
 */
add_task(async function test_header_toolbarseparator() {
  await gCUITestUtils.openMainMenu();

  let historyView = PanelMultiView.getViewNode(document, "PanelUI-history");
  document.getElementById("appMenu-history-button").click();
  await BrowserTestUtils.waitForEvent(historyView, "ViewShown");

  // Open Recently Closed Tabs and make sure there is a header separator
  let closedTabsView = PanelMultiView.getViewNode(
    document,
    RECENTLY_CLOSED_TABS_PANEL_ID
  );
  Assert.ok(!document.getElementById(RECENTLY_CLOSED_TABS_ITEM_ID).disabled);
  document.getElementById(RECENTLY_CLOSED_TABS_ITEM_ID).click();
  await BrowserTestUtils.waitForEvent(closedTabsView, "ViewShown");
  assertHeaderSeparator();

  // Go back and re-open the same view, header separator should be
  // re-added as well
  document
    .querySelector(`#${RECENTLY_CLOSED_TABS_PANEL_ID} .subviewbutton-back`)
    .click();
  await BrowserTestUtils.waitForEvent(historyView, "ViewShown");
  document.getElementById(RECENTLY_CLOSED_TABS_ITEM_ID).click();
  await BrowserTestUtils.waitForEvent(closedTabsView, "ViewShown");
  assertHeaderSeparator();

  await gCUITestUtils.hideMainMenu();
});
