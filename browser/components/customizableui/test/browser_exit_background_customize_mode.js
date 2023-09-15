"use strict";

/**
 * Tests that if customize mode is currently attached to a background
 * tab, and that tab browses to a new location, that customize mode
 * is detached from that tab.
 */
add_task(async function test_exit_background_customize_mode() {
  let nonCustomizingTab = gBrowser.selectedTab;

  Assert.equal(
    gBrowser.tabContainer.querySelector("tab[customizemode=true]"),
    null,
    "Should not have a tab marked as being the customize tab now."
  );

  await startCustomizing();
  is(gBrowser.tabs.length, 2, "Should have 2 tabs");

  let custTab = gBrowser.selectedTab;

  let finishedCustomizing = BrowserTestUtils.waitForEvent(
    gNavToolbox,
    "aftercustomization"
  );
  await BrowserTestUtils.switchTab(gBrowser, nonCustomizingTab);
  await finishedCustomizing;

  let newURL = "http://example.com/";
  BrowserTestUtils.startLoadingURIString(custTab.linkedBrowser, newURL);
  await BrowserTestUtils.browserLoaded(custTab.linkedBrowser, false, newURL);

  Assert.equal(
    gBrowser.tabContainer.querySelector("tab[customizemode=true]"),
    null,
    "Should not have a tab marked as being the customize tab now."
  );

  await startCustomizing();
  is(gBrowser.tabs.length, 3, "Should have 3 tabs now");

  await endCustomizing();
  BrowserTestUtils.removeTab(custTab);
});
