/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(function (win) {
    registerCleanupFunction(() => win.close());

    let cw = win.TabView.getContentWindow();
    let tab = win.gBrowser.tabs[0];
    let tabItem = tab._tabViewTabItem;
    let isIdle = false;

    // We replace UI.isIdle() here to not rely on setTimeout(). While this
    // function returns false (busy) we expect no tabItem updates to happen.
    let busyCount = 5;
    cw.UI.isIdle = function () {
      return isIdle = (0 > --busyCount);
    };

    cw.TabItems.pausePainting();

    tabItem.addSubscriber("updated", function onUpdated() {
      tabItem.removeSubscriber("updated", onUpdated);
      ok(isIdle, "tabItem is updated only when UI is idle");
      finish();
    });

    cw.TabItems.update(tab);
    cw.TabItems.resumePainting();
  });
}
