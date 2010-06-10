function press(key, expectedPos) {
  var originalSelectedTab = gBrowser.selectedTab;
  EventUtils.synthesizeKey("VK_" + key.toUpperCase(), { accelKey: true });
  is(gBrowser.selectedTab, originalSelectedTab,
     "accel+" + key + " doesn't change which tab is selected");
  is(gBrowser.tabContainer.selectedIndex, expectedPos,
     "accel+" + key + " moves the tab to the expected position");
  is(document.activeElement, gBrowser.selectedTab,
     "accel+" + key + " leaves the selected tab focused");
}

function test() {
  gBrowser.addTab();
  gBrowser.addTab();
  is(gBrowser.tabs.length, 3, "got three tabs");
  is(gBrowser.tabs[0], gBrowser.selectedTab, "first tab is selected");

  gBrowser.selectedTab.focus();
  is(document.activeElement, gBrowser.selectedTab, "selected tab is focused");

  press("right", 1);
  press("down", 2);
  press("left", 1);
  press("up", 0);
  press("end", 2);
  press("home", 0);

  gBrowser.removeCurrentTab();
  gBrowser.removeCurrentTab();
}
