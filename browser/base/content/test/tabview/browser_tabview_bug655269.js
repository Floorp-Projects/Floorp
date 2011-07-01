/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(function (win) {
    let cw = win.TabView.getContentWindow();
    let tabItem = win.gBrowser.tabs[0]._tabViewTabItem;

    tabItem.addSubscriber(tabItem, "savedCachedImageData", function () {
      tabItem.removeSubscriber(tabItem, "savedCachedImageData");

      ok(cw.UI.isDOMWindowClosing, "dom window is closing");
      waitForFocus(finish);
    });

    win.close();
  });
}
