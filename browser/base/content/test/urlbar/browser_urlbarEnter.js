/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_VALUE = "example.com/\xF7?\xF7";
const START_VALUE = "example.com/%C3%B7?%C3%B7";

add_task(async function returnKeypress() {
  info("Simple return keypress");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, START_VALUE);

  gURLBar.focus();
  EventUtils.synthesizeKey("KEY_Enter");
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  // Check url bar and selected tab.
  is(gURLBar.textValue, TEST_VALUE, "Urlbar should preserve the value on return keypress");
  is(gBrowser.selectedTab, tab, "New URL was loaded in the current tab");

  // Cleanup.
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function altReturnKeypress() {
  info("Alt+Return keypress");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, START_VALUE);

  let tabOpenPromise = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabOpen");
  gURLBar.focus();
  EventUtils.synthesizeKey("KEY_Enter", {altKey: true});

  // wait for the new tab to appear.
  await tabOpenPromise;

  // Check url bar and selected tab.
  is(gURLBar.textValue, TEST_VALUE, "Urlbar should preserve the value on return keypress");
  isnot(gBrowser.selectedTab, tab, "New URL was loaded in a new tab");

  // Cleanup.
  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
