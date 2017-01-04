/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  function testState(aPinned) {
    function elemAttr(id, attr) {
      return document.getElementById(id).getAttribute(attr);
    }

    if (aPinned) {
      is(elemAttr("key_close", "disabled"), "true",
         "key_close should be disabled when a pinned-tab is selected");
      is(elemAttr("menu_close", "key"), "",
         "menu_close shouldn't have a key set when a pinned is selected");
    } else {
      is(elemAttr("key_close", "disabled"), "",
         "key_closed shouldn't have disabled state set when a non-pinned tab is selected");
      is(elemAttr("menu_close", "key"), "key_close",
         "menu_close should have key_close set as its key when a non-pinned tab is selected");
    }
  }

  let lastSelectedTab = gBrowser.selectedTab;
  ok(!lastSelectedTab.pinned, "We should have started with a regular tab selected");

  testState(false);

  let pinnedTab = gBrowser.addTab("about:blank");
  gBrowser.pinTab(pinnedTab);

  // Just pinning the tab shouldn't change the key state.
  testState(false);

  // Test updating key state after selecting a tab.
  gBrowser.selectedTab = pinnedTab;
  testState(true);

  gBrowser.selectedTab = lastSelectedTab;
  testState(false);

  gBrowser.selectedTab = pinnedTab;
  testState(true);

  // Test updating the key state after un/pinning the tab.
  gBrowser.unpinTab(pinnedTab);
  testState(false);

  gBrowser.pinTab(pinnedTab);
  testState(true);

  // Test updating the key state after removing the tab.
  gBrowser.removeTab(pinnedTab);
  testState(false);

  finish();
}
