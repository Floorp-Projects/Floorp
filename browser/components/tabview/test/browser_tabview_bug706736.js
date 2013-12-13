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
    is(groupItemTwo.getChildren().length, 1, "Group two has 1 tab items");

    whenTabViewIsHidden(function() {
      win.gBrowser.removeTab(win.gBrowser.selectedTab);
      executeSoon(function() {
        win.undoCloseTab(0);

        groupItemTwo.addSubscriber("childAdded", function onChildAdded(data) {
          groupItemTwo.removeSubscriber("childAdded", onChildAdded);

          is(groupItemOne.getChildren().length, 1, "Group one still has 1 tab item");
          is(groupItemTwo.getChildren().length, 1, "Group two still has 1 tab item");
        });

        finish();
      });
    }, win);
    groupItemTwo.getChild(0).zoomIn();
  }, function(win) {
    let newTab = win.gBrowser.addTab();
    win.gBrowser.pinTab(newTab);
  });
}
