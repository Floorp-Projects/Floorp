/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the context menu of hidden tabs which users can open via the All Tabs
// menu's Hidden Tabs view.

add_task(async function test() {
  is(gBrowser.visibleTabs.length, 1, "there is initially one visible tab");

  let tab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.hideTab(tab);
  ok(tab.hidden, "new tab is hidden");
  is(gBrowser.visibleTabs.length, 1, "there is still only one visible tabs");

  updateTabContextMenu(tab);
  ok(
    document.getElementById("context_moveTabOptions").disabled,
    "Move Tab menu is disabled"
  );
  ok(
    document.getElementById("context_closeTabsToTheStart").disabled,
    "Close Tabs to Left is disabled"
  );
  ok(
    document.getElementById("context_closeTabsToTheEnd").disabled,
    "Close Tabs to Right is disabled"
  );
  ok(
    document.getElementById("context_reopenInContainer").disabled,
    "Open in New Container Tab menu is disabled"
  );
  gBrowser.removeTab(tab);
});
