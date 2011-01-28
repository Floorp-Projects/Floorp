/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let cw;
  let tab;

  let testReconnectWithSameUrl = function () {
    tab = gBrowser.loadOneTab('http://mochi.test:8888/', {inBackground: true});

    afterAllTabsLoaded(function () {
      let tabItem = tab._tabViewTabItem;
      let data = tabItem.getStorageData(true);
      gBrowser.removeTab(tab);

      cw.TabItems.pauseReconnecting();
      tab = gBrowser.loadOneTab('http://mochi.test:8888/', {inBackground: true});
      cw.Storage.saveTab(tab, data);

      whenTabAttrModified(tab, function () {
        tabItem = tab._tabViewTabItem;
        cw.TabItems.resumeReconnecting();
        ok(tabItem.isShowingCachedData(), 'tabItem shows cached data');

        testChangeUrlAfterReconnect();
      });
    });
  }

  let testChangeUrlAfterReconnect = function () {
    tab.linkedBrowser.loadURI('http://mochi.test:8888/browser/');

    whenTabAttrModified(tab, function () {
      cw.TabItems._update(tab);

      let tabItem = tab._tabViewTabItem;
      let currentLabel = tabItem.$tabTitle.text();

      is(currentLabel, 'mochitest index /browser/', 'tab label is up-to-date');
      testReconnectWithNewUrl();
    });
  }

  let testReconnectWithNewUrl = function () {
    let tabItem = tab._tabViewTabItem;
    let data = tabItem.getStorageData(true);
    gBrowser.removeTab(tab);

    cw.TabItems.pauseReconnecting();
    tab = gBrowser.loadOneTab('http://mochi.test:8888/', {inBackground: true});
    cw.Storage.saveTab(tab, data);

    whenTabAttrModified(tab, function () {
      tabItem = tab._tabViewTabItem;
      cw.TabItems.resumeReconnecting();
      ok(!tabItem.isShowingCachedData(), 'tabItem does not show cached data');

      gBrowser.removeTab(tab);
      hideTabView(finish);
    });
  }

  waitForExplicitFinish();

  showTabView(function () {
    cw = TabView.getContentWindow();
    testReconnectWithSameUrl();
  });
}

// ----------
function whenTabAttrModified(tab, callback) {
  let onModified = function (event) {
    if (tab === event.target) {
      container.removeEventListener('TabAttrModified', onModified, false);
      // we need executeSoon here because the tabItem also listens for the
      // onTabAttrModified event. so this is to make sure the tabItem logic
      // is executed before the test logic.
      executeSoon(callback);
    }
  }

  let container = gBrowser.tabContainer;
  container.addEventListener('TabAttrModified', onModified, false);
}
