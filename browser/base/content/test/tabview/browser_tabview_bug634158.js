/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let createOrphan = function (callback) {
    let tab = gBrowser.loadOneTab('about:blank', {inBackground: true});

    registerCleanupFunction(function () {
      gBrowser.removeTab(tab);
    });

    afterAllTabsLoaded(function () {
      let tabItem = tab._tabViewTabItem;
      tabItem.parent.remove(tabItem);
      callback(tabItem);
    });
  }

  waitForExplicitFinish();

  showTabView(function () {
    registerCleanupFunction(function () TabView.hide());

    createOrphan(function (tabItem) {
      let cw = TabView.getContentWindow();
      let container = cw.iQ(tabItem.container);
      let expander = cw.iQ('.expander', container);

      let bounds = container.bounds();
      let halfWidth = bounds.width / 2;
      let halfHeight = bounds.height / 2;

      let rect = new cw.Rect(bounds.left + halfWidth, bounds.top + halfHeight,
                          halfWidth, halfHeight);
      ok(rect.contains(expander.bounds()), "expander is in the tabItem's bottom-right corner");

      hideTabView(finish);
    });
  });
}
