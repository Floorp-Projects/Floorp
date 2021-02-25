/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function removeTabsToTheStart() {
  // don't mess with the original tab
  let originalTab = gBrowser.selectedTab;
  gBrowser.pinTab(originalTab);

  // Add three new tabs after the original tab. Pin the second one.
  let firstTab = await addTab();
  let pinnedTab = await addTab();
  let lastTab = await addTab();
  gBrowser.pinTab(pinnedTab);

  // Check that there is only one closable tab from lastTab to the start
  is(
    gBrowser.getTabsToTheStartFrom(lastTab).length,
    1,
    "One unpinned tab towards the start"
  );

  // Remove tabs to the start
  gBrowser.removeTabsToTheStartFrom(lastTab);

  ok(firstTab.closing, "First tab is closing");
  ok(!pinnedTab.closing, "Pinned tab is not closing");
  ok(!lastTab.closing, "Last tab is not closing");

  // cleanup
  gBrowser.unpinTab(originalTab);
  for (let tab of [pinnedTab, lastTab]) {
    BrowserTestUtils.removeTab(tab);
  }
});
