/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let contentWindow;
let groupItemTwoId;

function test() {
  waitForExplicitFinish();
  
  registerCleanupFunction(function() {
    while (gBrowser.tabs[1])
      gBrowser.removeTab(gBrowser.tabs[1]);
    hideTabView();
  });
  gBrowser.loadOneTab("about:blank", { inBackground: true });
  showTabView(setup);
}

function setup() {
  registerCleanupFunction(function() {
    let groupItem = contentWindow.GroupItems.groupItem(groupItemTwoId);
    if (groupItem)
      closeGroupItem(groupItem);
  });

  let contentWindow = TabView.getContentWindow();
  is(contentWindow.GroupItems.groupItems.length, 1, "Has only one group");

  let groupItemOne = contentWindow.GroupItems.groupItems[0];
  is(groupItemOne.getChildren().length, 2, "Group one has 2 tab items");

  let groupItemTwo = createGroupItemWithBlankTabs(window, 250, 250, 40, 1);
  groupItemTwoId = groupItemTwo.id;
  testGroups(groupItemOne, groupItemTwo, contentWindow);
}

function testGroups(groupItemOne, groupItemTwo, contentWindow) {
  // check active tab and group
  is(contentWindow.GroupItems.getActiveGroupItem(), groupItemTwo, 
     "The group two is the active group");
  is(contentWindow.UI.getActiveTab(), groupItemTwo.getChild(0), 
     "The first tab item in group two is active");
  
  let tabItem = groupItemOne.getChild(1);
  tabItem.addSubscriber("tabRemoved", function onTabRemoved() {
    tabItem.removeSubscriber("tabRemoved", onTabRemoved);

    is(groupItemOne.getChildren().length, 1,
      "The num of childen in group one is 1");

    // check active group and active tab
    is(contentWindow.GroupItems.getActiveGroupItem(), groupItemOne, 
       "The group one is the active group");
    is(contentWindow.UI.getActiveTab(), groupItemOne.getChild(0), 
       "The first tab item in group one is active");

    whenTabViewIsHidden(function() {
      is(groupItemOne.getChildren().length, 2, 
         "The num of childen in group one is 2");

      // clean up and finish
      closeGroupItem(groupItemTwo, function() {
        gBrowser.removeTab(groupItemOne.getChild(1).tab);
        is(contentWindow.GroupItems.groupItems.length, 1, "Has only one group");
        is(groupItemOne.getChildren().length, 1, 
           "The num of childen in group one is 1");
        is(gBrowser.tabs.length, 1, "Has only one tab");

        finish();
      });
    });
    EventUtils.synthesizeKey("t", { accelKey: true });
  });
  // close a tab item in group one
   EventUtils.synthesizeMouseAtCenter(tabItem.$close[0], {}, contentWindow);
}
