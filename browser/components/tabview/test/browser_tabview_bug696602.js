/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
let win;

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(function(newWin) {
    win = newWin;

    registerCleanupFunction(function() {
      win.close();
    });

    let cw = win.TabView.getContentWindow();
    let groupItemOne = cw.GroupItems.groupItems[0];
    let groupItemTwo = createGroupItemWithBlankTabs(win, 300, 300, 20, 10);

    is(win.gBrowser.tabContainer.getAttribute("overflow"), "true",
       "The tabstrip is overflow");

    is(groupItemOne.getChildren().length, 10, "Group one has 10 tabs");
    is(groupItemTwo.getChildren().length, 10, "Group two has 10 tabs");

    checkSelectedTabVisible("two", function() {
      showTabView(function() {
        checkSelectedTabVisible("one", finish);
        groupItemOne.getChild(9).zoomIn();
      }, win);
    });
    groupItemTwo.getChild(9).zoomIn();
  }, function(newWin) {
    for (let i = 0; i < 9; i++)
      newWin.gBrowser.addTab();
  });
}

function isSelectedTabVisible() {
  let tabstrip = win.gBrowser.tabContainer.mTabstrip;
  let scrollRect = tabstrip.scrollClientRect;
  let tab = win.gBrowser.selectedTab.getBoundingClientRect();

  return (scrollRect.left <= tab.left && tab.right <= scrollRect.right);
}

function checkSelectedTabVisible(groupName, callback) {
  whenTabViewIsHidden(function() {
    ok(isSelectedTabVisible(), "Group " + groupName + " selected tab is visible");
    callback();
  }, win);
}
