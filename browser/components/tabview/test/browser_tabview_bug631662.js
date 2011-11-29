/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let cw;
  let thumbnailUpdateCount = 0;

  function actionAddTab() {
    let count = thumbnailUpdateCount;
    addUpdateListener(gBrowser.addTab("about:home"));

    afterAllTabsLoaded(function () {
      is(thumbnailUpdateCount, count, "add-tab: tabs were not updated");
      next();
    });
  }

  function actionMoveTab() {
    let count = thumbnailUpdateCount;
    gBrowser.moveTabTo(gBrowser.tabs[0], 1);
    gBrowser.moveTabTo(gBrowser.tabs[1], 0);
    is(thumbnailUpdateCount, count, "move-tab: tabs were not updated");
    next();
  }

  function actionSelectTab() {
    let count = thumbnailUpdateCount;
    gBrowser.selectedTab = gBrowser.tabs[1]
    gBrowser.selectedTab = gBrowser.tabs[0]
    is(thumbnailUpdateCount, count, "select-tab: tabs were not updated");
    next();
  }

  function actionRemoveTab() {
    let count = thumbnailUpdateCount;
    gBrowser.removeTab(gBrowser.tabs[1]);
    is(thumbnailUpdateCount, count, "remove-tab: tabs were not updated");
    next();
  }

  function addUpdateListener(tab) {
    let tabItem = tab._tabViewTabItem;

    function onUpdate() thumbnailUpdateCount++;
    tabItem.addSubscriber("thumbnailUpdated", onUpdate);

    registerCleanupFunction(function () {
      tabItem.removeSubscriber("thumbnailUpdated", onUpdate)
    });
  }

  function finishTest() {
    let count = thumbnailUpdateCount;

    showTabView(function () {
      isnot(thumbnailUpdateCount, count, "finish: tabs were updated");
      hideTabView(finish);
    });
  }

  let actions = [
    actionAddTab, actionMoveTab, actionSelectTab, actionRemoveTab
  ];

  function next() {
    let action = actions.shift();

    if (action) {
      action();
    } else {
      finishTest();
    }
  }

  waitForExplicitFinish();

  showTabView(function () {
    cw = TabView.getContentWindow();
    hideTabView(function () {
      addUpdateListener(gBrowser.tabs[0]);
      next();
    });
  });
}
