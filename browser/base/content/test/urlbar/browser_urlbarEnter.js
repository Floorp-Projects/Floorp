/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_VALUE = "example.com/\xF7?\xF7";
const START_VALUE = "example.com/%C3%B7?%C3%B7";

add_task(function* () {
  info("Simple return keypress");
  let tab = gBrowser.selectedTab = gBrowser.addTab(START_VALUE);

  gURLBar.focus();
  EventUtils.synthesizeKey("VK_RETURN", {});
  yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  // Check url bar and selected tab.
  is(gURLBar.textValue, TEST_VALUE, "Urlbar should preserve the value on return keypress");
  is(gBrowser.selectedTab, tab, "New URL was loaded in the current tab");

  // Cleanup.
  gBrowser.removeCurrentTab();
});

add_task(function* () {
  info("Alt+Return keypress");
  let tab = gBrowser.selectedTab = gBrowser.addTab(START_VALUE);
  // due to bug 691608, we must wait for the load event, else isTabEmpty() will
  // return true on e10s for this tab, so it will be reused even with altKey.
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  gURLBar.focus();
  EventUtils.synthesizeKey("VK_RETURN", {altKey: true});

  // wait for the new tab to appear.
  yield BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabOpen");

  // Check url bar and selected tab.
  is(gURLBar.textValue, TEST_VALUE, "Urlbar should preserve the value on return keypress");
  isnot(gBrowser.selectedTab, tab, "New URL was loaded in a new tab");

  // Cleanup.
  gBrowser.removeTab(tab);
  gBrowser.removeCurrentTab();
});
