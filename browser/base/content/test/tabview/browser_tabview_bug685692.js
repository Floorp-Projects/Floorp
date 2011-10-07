/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  gBrowser.addTab("http://example.com/");
  gBrowser.addTab("http://example.com/");

  registerCleanupFunction(function () {
    while (gBrowser.tabs.length > 1)
      gBrowser.removeTab(gBrowser.tabs[1]);
    hideTabView();
  })

  afterAllTabsLoaded(function() {
    showTabView(function() {
      let cw = TabView.getContentWindow();

      let groupItemOne = cw.GroupItems.groupItems[0];
      is(groupItemOne.getChildren().length, 3, "The number of tabs in group one is 3");
 
      // create a group with a blank tab
      let groupItemTwo = createGroupItemWithBlankTabs(window, 400, 400, 40, 1);
      is(groupItemTwo.getChildren().length, 1, "The number of tabs in group two is 1");

      cw.UI.setActive(groupItemOne);

      moveTabToAnotherGroup(groupItemOne.getChild(2).tab, groupItemOne, groupItemTwo, function() {
        moveTabToAnotherGroup(groupItemOne.getChild(1).tab, groupItemOne, groupItemTwo, function() {
          cw.UI.setActive(groupItemOne);
          hideTabView(finish);
        });
      });
    });
  });
}

function moveTabToAnotherGroup(targetTab, groupItemOne, groupItemTwo, callback) {
  hideTabView(function() {
    let tabCountInGroupItemOne = groupItemOne.getChildren().length;
    let tabCountInGroupItemTwo = groupItemTwo.getChildren().length;

    TabView.moveTabTo(targetTab, groupItemTwo.id);

    showTabView(function() {
      is(groupItemOne.getChildren().length, --tabCountInGroupItemOne, "The number of tab items in group one is decreased");
      is(groupItemTwo.getChildren().length, ++tabCountInGroupItemTwo, "The number of tab items in group two is increased");
      is(groupItemTwo.getChild(tabCountInGroupItemTwo-1).tab, targetTab, "The last tab is the moved tab");

      callback();
    });
  });
}
