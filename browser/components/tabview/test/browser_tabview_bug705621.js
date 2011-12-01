/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(function(win) {
    registerCleanupFunction(function() {
      win.close();
    });

    let cw = win.TabView.getContentWindow();

    let groupItemOne = cw.GroupItems.groupItems[0];
    is(groupItemOne.getChildren().length, 1, "Group one has 1 tab item");

    let groupItemTwo = createGroupItemWithBlankTabs(win, 300, 300, 40, 1);
    is(groupItemTwo.getChildren().length, 1, "Group two has 2 tab items");

    whenTabViewIsHidden(function() {
      executeSoon(function() { 
        win.gBrowser.removeTab(win.gBrowser.selectedTab);
        is(cw.UI.getActiveTab(), groupItemOne.getChild(0), "TabItem in Group one is selected");
        finish();
      });
    }, win);
    groupItemTwo.getChild(0).zoomIn();
  });
}

