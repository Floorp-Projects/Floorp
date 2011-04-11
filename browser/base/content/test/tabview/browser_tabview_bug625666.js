/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let cw;

  let getTabItemAspect = function (tabItem) {
    let bounds = cw.iQ('.thumb', tabItem.container).bounds();
    let padding = cw.TabItems.tabItemPadding;
    return (bounds.height + padding.y) / (bounds.width + padding.x);
  }

  let getAspectRange = function () {
    let aspect = cw.TabItems.tabAspect;
    let variance = aspect / 100 * 1.5;
    return new cw.Range(aspect - variance, aspect + variance);
  }

  waitForExplicitFinish();

  newWindowWithTabView(function (win) {
    registerCleanupFunction(function () win.close());
    cw = win.TabView.getContentWindow();

    // prepare orphan tab
    let tabItem = win.gBrowser.tabs[0]._tabViewTabItem;
    tabItem.parent.remove(tabItem, {immediately: true});
    tabItem.setBounds(new cw.Rect(20, 20, 200, 165), true);

    let bounds = tabItem.getBounds();

    // prepare group item
    let box = new cw.Rect(20, 300, 400, 200);
    let groupItem = new cw.GroupItem([], {bounds: box, immediately: true});

    groupItem.setBounds(new cw.Rect(20, 100, 400, 200));
    groupItem.pushAway(true);

    let newBounds = tabItem.getBounds();
    ok(newBounds.width < bounds.width, "The new width of item is smaller than the old one.");
    ok(newBounds.height < bounds.height, "The new height of item is smaller than the old one.");

    let aspectRange = getAspectRange();
    let aspect = getTabItemAspect(tabItem);
    ok(aspectRange.contains(aspect), "orphaned tabItem's aspect is correct");

    finish();
  });
}
