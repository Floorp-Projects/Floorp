/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let originalTab;
let newTabOne;
let groupItemTwoId;

function test() {
  waitForExplicitFinish();

  originalTab = gBrowser.visibleTabs[0];
  // add a tab to the existing group.
  newTabOne = gBrowser.addTab();

  registerCleanupFunction(function() {
    while (gBrowser.tabs[1])
      gBrowser.removeTab(gBrowser.tabs[1]);
    hideTabView();
  });

  showTabView(function() {
    let contentWindow = TabView.getContentWindow();

    registerCleanupFunction(function() {
      let groupItem = contentWindow.GroupItems.groupItem(groupItemTwoId);
      if (groupItem)
        closeGroupItem(groupItem);
    });

    is(contentWindow.GroupItems.groupItems.length, 1, 
       "There is one group item on startup");
    let groupItemOne = contentWindow.GroupItems.groupItems[0];
    is(groupItemOne.getChildren().length, 2, 
       "There should be two tab items in that group.");
    is(gBrowser.selectedTab, groupItemOne.getChild(0).tab,
       "The currently selected tab should be the first tab in the groupItemOne");

    // create another group with a tab.
    let groupItemTwo = createGroupItemWithBlankTabs(window, 300, 300, 200, 1);
    groupItemTwoId = groupItemTwoId;
    hideTabView(function() {
      // start the test
      testGroupSwitch(contentWindow, groupItemOne, groupItemTwo);
    });
  });
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
