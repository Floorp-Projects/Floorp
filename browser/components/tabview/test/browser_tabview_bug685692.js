/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(win => {
    win.gBrowser.addTab("http://example.com/");
    win.gBrowser.addTab("http://example.com/");

    afterAllTabsLoaded(function() {
      let cw = win.TabView.getContentWindow();

      let groupItemOne = cw.GroupItems.groupItems[0];
      is(groupItemOne.getChildren().length, 3, "The number of tabs in group one is 3");

      // create a group with a blank tab
      let groupItemTwo = createGroupItemWithBlankTabs(win, 400, 400, 40, 1);
      is(groupItemTwo.getChildren().length, 1, "The number of tabs in group two is 1");

      cw.UI.setActive(groupItemOne);

      moveTabToAnotherGroup(win, groupItemOne.getChild(2).tab, groupItemOne, groupItemTwo, function() {
        moveTabToAnotherGroup(win, groupItemOne.getChild(1).tab, groupItemOne, groupItemTwo, function() {
          groupItemOne.close();
          promiseWindowClosed(win).then(finish);
        });
      });
    });
  });
}

function moveTabToAnotherGroup(win, targetTab, groupItemOne, groupItemTwo, callback) {
  hideTabView(function() {
    let tabCountInGroupItemOne = groupItemOne.getChildren().length;
    let tabCountInGroupItemTwo = groupItemTwo.getChildren().length;

    win.TabView.moveTabTo(targetTab, groupItemTwo.id);

    showTabView(function() {
      is(groupItemOne.getChildren().length, --tabCountInGroupItemOne, "The number of tab items in group one is decreased");
      is(groupItemTwo.getChildren().length, ++tabCountInGroupItemTwo, "The number of tab items in group two is increased");
      is(groupItemTwo.getChild(tabCountInGroupItemTwo-1).tab, targetTab, "The last tab is the moved tab");

      callback();
    }, win);
  }, win);
}
