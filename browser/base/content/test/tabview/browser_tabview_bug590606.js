/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let originalTab;
let newTabOne;

function test() {
  waitForExplicitFinish();

  originalTab = gBrowser.visibleTabs[0];
  // add a tab to the existing group.
  newTabOne = gBrowser.addTab();

  let onTabviewShown = function() {
    window.removeEventListener("tabviewshown", onTabviewShown, false);

    let contentWindow = document.getElementById("tab-view").contentWindow;

    is(contentWindow.GroupItems.groupItems.length, 1, 
       "There is one group item on startup");
    let groupItemOne = contentWindow.GroupItems.groupItems[0];
    is(groupItemOne.getChildren().length, 2, 
       "There should be two tab items in that group.");
    is(gBrowser.selectedTab, groupItemOne.getChild(0).tab,
       "The currently selected tab should be the first tab in the groupItemOne");

    // create another group with a tab.
    let groupItemTwo = createEmptyGroupItem(contentWindow, 300, 300, 200);

    let onTabViewHidden = function() {
      window.removeEventListener("tabviewhidden", onTabViewHidden, false);
      // start the test
      testGroupSwitch(contentWindow, groupItemOne, groupItemTwo);
    };
    window.addEventListener("tabviewhidden", onTabViewHidden, false);

    // click on the + button
    let newTabButton = groupItemTwo.container.getElementsByClassName("newTabButton");
    ok(newTabButton[0], "New tab button exists");
    EventUtils.sendMouseEvent({ type: "click" }, newTabButton[0], contentWindow);
  };
  window.addEventListener("tabviewshown", onTabviewShown, false);
  TabView.toggle();
}

function testGroupSwitch(contentWindow, groupItemOne, groupItemTwo) {
  is(gBrowser.selectedTab, groupItemTwo.getChild(0).tab,
     "The currently selected tab should be the only tab in the groupItemTwo");

  // switch to groupItemOne
  let tabItem = contentWindow.GroupItems.getNextGroupItemTab(false);
  if (tabItem)
    gBrowser.selectedTab = tabItem.tab;
  is(gBrowser.selectedTab, groupItemOne.getChild(0).tab,
    "The currently selected tab should be the first tab in the groupItemOne");
  
  // switch to the second tab in groupItemOne
  gBrowser.selectedTab = groupItemOne.getChild(1).tab;

  // switch to groupItemTwo
  tabItem = contentWindow.GroupItems.getNextGroupItemTab(false);
  if (tabItem)
    gBrowser.selectedTab = tabItem.tab;
  is(gBrowser.selectedTab, groupItemTwo.getChild(0).tab,
    "The currently selected tab should be the only tab in the groupItemTwo");

  // switch to groupItemOne
  tabItem = contentWindow.GroupItems.getNextGroupItemTab(false);
  if (tabItem)
    gBrowser.selectedTab = tabItem.tab;
  is(gBrowser.selectedTab, groupItemOne.getChild(1).tab,
    "The currently selected tab should be the second tab in the groupItemOne");

  // cleanup.
  gBrowser.removeTab(groupItemTwo.getChild(0).tab);
  gBrowser.removeTab(newTabOne);

  finish();
}
