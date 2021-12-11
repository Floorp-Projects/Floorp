/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function removeTabsToTheEnd() {
  // Add three new tabs after the original tab. Pin the second one.
  let firstTab = await addTab();
  let pinnedTab = await addTab();
  let lastTab = await addTab();
  gBrowser.pinTab(pinnedTab);

  // Check that there is only one closable tab from firstTab to the end
  is(
    gBrowser.getTabsToTheEndFrom(firstTab).length,
    1,
    "One unpinned tab towards the end"
  );

  // Remove tabs to the end
  gBrowser.removeTabsToTheEndFrom(firstTab);

  ok(!firstTab.closing, "First tab is not closing");
  ok(!pinnedTab.closing, "Pinned tab is not closing");
  ok(lastTab.closing, "Last tab is closing");

  // cleanup
  for (let tab of [firstTab, pinnedTab]) {
    BrowserTestUtils.removeTab(tab);
  }
});
