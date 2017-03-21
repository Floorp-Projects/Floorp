"use strict";

/**
 * Tests that if customize mode is currently attached to a background
 * tab, and that tab browses to a new location, that customize mode
 * is detached from that tab.
 */
add_task(function* test_exit_background_customize_mode() {
  let nonCustomizingTab = gBrowser.selectedTab;

  Assert.equal(gBrowser.tabContainer.querySelector("tab[customizemode=true]"),
               null,
               "Should not have a tab marked as being the customize tab now.");

  yield startCustomizing();
  is(gBrowser.tabs.length, 2, "Should have 2 tabs");

  let custTab = gBrowser.selectedTab;

  let finishedCustomizing = BrowserTestUtils.waitForEvent(gNavToolbox, "aftercustomization");
  yield BrowserTestUtils.switchTab(gBrowser, nonCustomizingTab);
  yield finishedCustomizing;

  custTab.linkedBrowser.loadURI("http://example.com");
  yield BrowserTestUtils.browserLoaded(custTab.linkedBrowser);

  Assert.equal(gBrowser.tabContainer.querySelector("tab[customizemode=true]"),
               null,
               "Should not have a tab marked as being the customize tab now.");

  yield startCustomizing();
  is(gBrowser.tabs.length, 3, "Should have 3 tabs now");

  yield endCustomizing();
  yield BrowserTestUtils.removeTab(custTab);
});
