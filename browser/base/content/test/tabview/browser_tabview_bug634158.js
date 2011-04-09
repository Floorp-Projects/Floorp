/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(function (win) {
    registerCleanupFunction(function () {
      if (!win.closed)
        win.close();
    });

    let tabItem = win.gBrowser.tabs[0]._tabViewTabItem;
    tabItem.parent.remove(tabItem);

    let cw = win.TabView.getContentWindow();
    let container = cw.iQ(tabItem.container);
    let expander = cw.iQ('.expander', container);

    let bounds = container.bounds();
    let halfWidth = bounds.width / 2;
    let halfHeight = bounds.height / 2;

    let rect = new cw.Rect(bounds.left + halfWidth, bounds.top + halfHeight,
                        halfWidth, halfHeight);
    ok(rect.contains(expander.bounds()), "expander is in the tabItem's bottom-right corner");

    win.close();
    finish();
  });
}
