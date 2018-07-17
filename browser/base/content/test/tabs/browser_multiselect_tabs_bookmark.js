/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const PREF_MULTISELECT_TABS = "browser.tabs.multiselect";

async function addTab_example_com() {
  const tab = BrowserTestUtils.addTab(gBrowser,
    "http://example.com/", { skipAnimation: true });
  const browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);
  return tab;
}

add_task(async function setPref() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_MULTISELECT_TABS, true]]
  });
});

add_task(async function test() {
  let tab1 = await addTab();
  let tab2 = await addTab();
  let tab3 = await addTab();

  let menuItemBookmarkAllTabs = document.getElementById("context_bookmarkAllTabs");
  let menuItemBookmarkSelectedTabs = document.getElementById("context_bookmarkSelectedTabs");

  is(gBrowser.multiSelectedTabsCount, 0, "Zero multiselected tabs");

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  await triggerClickOn(tab2, { ctrlKey: true });

  ok(tab1.multiselected, "Tab1 is multiselected");
  ok(tab2.multiselected, "Tab2 is multiselected");
  ok(!tab3.multiselected, "Tab3 is not multiselected");

  // Check the context menu with a non-multiselected tab
  updateTabContextMenu(tab3);
  is(menuItemBookmarkAllTabs.hidden, false, "Bookmark All Tabs is visible");
  is(menuItemBookmarkSelectedTabs.hidden, true, "Bookmark Selected Tabs is hidden");

  // Check the context menu with a multiselected tab and one unique page in the selection.
  updateTabContextMenu(tab2);
  is(menuItemBookmarkAllTabs.hidden, true, "Bookmark All Tabs is visible");
  is(menuItemBookmarkSelectedTabs.hidden, false, "Bookmark Selected Tabs is hidden");
  is(PlacesCommandHook.uniqueSelectedPages.length, 1, "No more than one unique selected page");

  info("Add a different page to selection");
  let tab4 = await addTab_example_com();
  await triggerClickOn(tab4, { ctrlKey: true });

  ok(tab4.multiselected, "Tab4 is multiselected");
  is(gBrowser.multiSelectedTabsCount, 3, "Three multiselected tabs");

  // Check the context menu with a multiselected tab and two unique pages in the selection.
  updateTabContextMenu(tab2);
  is(menuItemBookmarkAllTabs.hidden, true, "Bookmark All Tabs is visible");
  is(menuItemBookmarkSelectedTabs.hidden, false, "Bookmark Selected Tabs is hidden");
  is(PlacesCommandHook.uniqueSelectedPages.length, 2, "More than one unique selected page");

  for (let tab of [tab1, tab2, tab3, tab4])
    BrowserTestUtils.removeTab(tab);
});
