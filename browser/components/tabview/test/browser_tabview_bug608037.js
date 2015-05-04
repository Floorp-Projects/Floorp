/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let tabOne;
let tabTwo;

function test() {
  waitForExplicitFinish();

  tabOne = gBrowser.addTab("http://mochi.test:8888/");
  tabTwo = gBrowser.addTab("http://mochi.test:8888/browser/browser/components/tabview/test/dummy_page.html");

  afterAllTabsLoaded(function () {
    // make sure the tab one is selected because undoCloseTab() would remove
    // the selected tab if it's a blank tab.
    gBrowser.selectedTab = tabOne;
    showTabView(onTabViewWindowLoaded);
  });
}

function onTabViewWindowLoaded() {
  let contentWindow = TabView.getContentWindow();
  let groupItems = contentWindow.GroupItems.groupItems;
  is(groupItems.length, 1, "There is only one group");
  is(groupItems[0].getChildren().length, 3, "The group has three tab items");

  BrowserTestUtils.removeTab(tabTwo).then(() => {
    ok(TabView.isVisible(), "Tab View is still visible after removing a tab");
    is(groupItems[0].getChildren().length, 2, "The group has two tab items");

    restoreTab(function (tabTwo) {
      ok(TabView.isVisible(), "Tab View is still visible after restoring a tab");
      is(groupItems[0].getChildren().length, 3, "The group still has three tab items");

      // clean up and finish
      hideTabView(function () {
        gBrowser.removeTab(tabOne);
        gBrowser.removeTab(tabTwo);
        finish();
      });
    });
  });
}
