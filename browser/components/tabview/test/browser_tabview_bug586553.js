/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  TabView.toggle();
}

let moves = 0;
let contentWindow = null;
let newTabs = [];
let originalTab = null;

function onTabMove(e) {
  let tab = e.target;
  moves++;
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);
  
  contentWindow = document.getElementById("tab-view").contentWindow;
  
  originalTab = gBrowser.selectedTab;
  newTabs = [gBrowser.addTab("about:rights"), gBrowser.addTab("about:mozilla"), gBrowser.addTab("about:license")];

  is(originalTab._tPos, 0, "Original tab is in position 0");
  is(newTabs[0]._tPos, 1, "Rights is in position 1");
  is(newTabs[1]._tPos, 2, "Mozilla is in position 2");
  is(newTabs[2]._tPos, 3, "License is in position 3");
  
  gBrowser.tabContainer.addEventListener("TabMove", onTabMove, false);
    
  let groupItem = contentWindow.GroupItems.getActiveGroupItem();
  
  // move 3 > 0 (and therefore 0 > 1, 1 > 2, 2 > 3)
  groupItem._children.splice(0, 0, groupItem._children.splice(3, 1)[0]);
  groupItem.arrange();
  
  window.addEventListener("tabviewhidden", onTabViewWindowHidden, false);
  TabView.toggle();
}

function onTabViewWindowHidden() {
  window.removeEventListener("tabviewhidden", onTabViewWindowHidden, false);
  gBrowser.tabContainer.removeEventListener("TabMove", onTabMove, false);
  
  is(moves, 1, "Only one move should be necessary for this basic move.");

  is(newTabs[2]._tPos, 0, "License is in position 0");
  is(originalTab._tPos, 1, "Original tab is in position 1");
  is(newTabs[0]._tPos, 2, "Rights is in position 2");
  is(newTabs[1]._tPos, 3, "Mozilla is in position 3");
  
  gBrowser.removeTab(newTabs[0]);
  gBrowser.removeTab(newTabs[1]);
  gBrowser.removeTab(newTabs[2]);
  finish();
}
