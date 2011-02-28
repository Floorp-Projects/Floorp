/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that groups behave properly when closing all tabs but app tabs.

let appTab, contentWindow;
let originalGroup, originalGroupTab, newGroup, newGroupTab;

function test() {
  waitForExplicitFinish();

  appTab = gBrowser.selectedTab;
  gBrowser.pinTab(appTab);
  originalGroupTab = gBrowser.addTab();

  addEventListener("tabviewshown", createGroup, false);
  TabView.toggle();
}

function createGroup() {
  removeEventListener("tabviewshown", createGroup, false);

  contentWindow = document.getElementById("tab-view").contentWindow;
  is(contentWindow.GroupItems.groupItems.length, 1, "There's only one group");

  originalGroup = contentWindow.GroupItems.groupItems[0];

  // Create a new group.
  let box = new contentWindow.Rect(20, 400, 300, 300);
  newGroup = new contentWindow.GroupItem([], { bounds: box });

  contentWindow.GroupItems.setActiveGroupItem(newGroup);

  addEventListener("tabviewhidden", addTab, false);
  TabView.toggle();
}

function addTab() {
  removeEventListener("tabviewhidden", addTab, false);

  newGroupTab = gBrowser.addTab();
  is(newGroup.getChildren().length, 1, "One tab is in the new group");
  executeSoon(removeTab);
}

function removeTab() {
  is(gBrowser.visibleTabs.length, 2, "There are two tabs displayed");
  gBrowser.removeTab(newGroupTab);

  is(newGroup.getChildren().length, 0, "No tabs are in the new group");
  is(gBrowser.visibleTabs.length, 1, "There is one tab displayed");
  is(contentWindow.GroupItems.groupItems.length, 2,
     "There are two groups still");

  addEventListener("tabviewshown", checkForRemovedGroup, false);
  TabView.toggle();
}

function checkForRemovedGroup() {
  removeEventListener("tabviewshown", checkForRemovedGroup, false);

  is(contentWindow.GroupItems.groupItems.length, 1,
     "There is now only one group");

  addEventListener("tabviewhidden", finishTest, false);
  TabView.toggle();
}

function finishTest() {
  removeEventListener("tabviewhidden", finishTest, false);

  gBrowser.removeTab(originalGroupTab);
  gBrowser.unpinTab(appTab);
  finish();
}

