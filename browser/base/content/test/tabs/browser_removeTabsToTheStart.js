/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function removeTabsToTheStart() {
  // Add two new tabs after the original tab. Pin the first one.
  let originalTab = gBrowser.selectedTab;
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

  ok(originalTab.closing, "Original tab is closing");
  ok(!pinnedTab.closing, "Pinned tab is not closing");
  ok(!lastTab.closing, "Last tab is not closing");

  // we closed the original tab, so add a new one before cleaning up
  await addTab();
  for (let tab of [pinnedTab, lastTab]) {
    BrowserTestUtils.removeTab(tab);
  }
});
