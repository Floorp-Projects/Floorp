/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  function testState(aPinned) {
    function elemAttr(id, attr) {
      return document.getElementById(id).getAttribute(attr);
    }

    is(elemAttr("key_close", "disabled"), "",
       "key_closed should always be enabled");
    is(elemAttr("menu_close", "key"), "key_close",
       "menu_close should always have key_close set");
  }

  let unpinnedTab = gBrowser.selectedTab;
  ok(!unpinnedTab.pinned, "We should have started with a regular tab selected");

  testState(false);

  let pinnedTab = gBrowser.addTab();
  gBrowser.pinTab(pinnedTab);

  // Just pinning the tab shouldn't change the key state.
  testState(false);

  // Test key state after selecting a tab.
  gBrowser.selectedTab = pinnedTab;
  testState(true);

  gBrowser.selectedTab = unpinnedTab;
  testState(false);

  gBrowser.selectedTab = pinnedTab;
  testState(true);

  // Test the key state after un/pinning the tab.
  gBrowser.unpinTab(pinnedTab);
  testState(false);

  gBrowser.pinTab(pinnedTab);
  testState(true);

  // Test that accel+w in a pinned tab selects the next tab.
  let pinnedTab2 = gBrowser.addTab();
  gBrowser.pinTab(pinnedTab2);
  gBrowser.selectedTab = pinnedTab;

  EventUtils.synthesizeKey("w", { accelKey: true });
  is(gBrowser.tabs.length, 3, "accel+w in a pinned tab didn't close it");
  is(gBrowser.selectedTab, unpinnedTab, "accel+w in a pinned tab selected the first unpinned tab");

  // Test the key state after removing the tab.
  gBrowser.removeTab(pinnedTab);
  gBrowser.removeTab(pinnedTab2);
  testState(false);

  finish();
}
