/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let checkUpdateTimes = function (groupItem) {
    let children = groupItem.getChildren();
    let earliestUpdateTime = children.shift()._testLastTabUpdateTime;

    children.forEach(function (tabItem) {
      let updateTime = tabItem._testLastTabUpdateTime;
      ok(earliestUpdateTime <= updateTime, "Stacked group item update (" +
         updateTime + ") > first item (" + earliestUpdateTime + ")");
    });
  }

  waitForExplicitFinish();

  newWindowWithTabView(function (win) {
    registerCleanupFunction(() => win.close());

    let numTabsToUpdate = 10;
    let groupItem = createGroupItemWithBlankTabs(win, 150, 150, 100, numTabsToUpdate, false);
    ok(groupItem.isStacked(), "groupItem is stacked");

    let cw = win.TabView.getContentWindow();
    cw.TabItems.pausePainting();

    groupItem.getChildren().forEach(function (tabItem) {
      tabItem.addSubscriber("updated", function onUpdated() {
        tabItem.removeSubscriber("updated", onUpdated);
        tabItem._testLastTabUpdateTime = tabItem._lastTabUpdateTime;

        if (--numTabsToUpdate)
          return;

        checkUpdateTimes(groupItem);
        finish();
      });

      cw.TabItems.update(tabItem.tab);
    });

    cw.TabItems.resumePainting();
  });
}
