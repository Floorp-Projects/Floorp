/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let numChildren = 7;

  let assertTopOfStack = function (tabItem) {
    ok(!tabItem.getHidden(), "tabItem is visible");
    is(tabItem.zIndex, tabItem.parent.getZ() + numChildren + 1, "zIndex");
  }

  let testTopOfStack = function (tabItem, callback) {
    hideTabView(function () {
      gBrowser.selectedTab = tabItem.tab;

      showTabView(function () {
        assertTopOfStack(tabItem);
        callback();
      });
    });
  }

  let finishTest = function () {
    registerCleanupFunction(function () {
      is(1, gBrowser.tabs.length, "there is one tab, only");
      ok(!TabView.isVisible(), "tabview is not visible");
    });

    finish();
  }

  waitForExplicitFinish();

  showTabView(function () {
    let groupItem = createGroupItemWithBlankTabs(window, 150, 150, 10, numChildren);

    registerCleanupFunction(function () {
      closeGroupItem(groupItem, () => TabView.hide());
    });

    testTopOfStack(groupItem.getChild(1), function () {
      testTopOfStack(groupItem.getChild(6), function () {
        closeGroupItem(groupItem, function () {
          hideTabView(finishTest);
        });
      });
    });
  });
}
