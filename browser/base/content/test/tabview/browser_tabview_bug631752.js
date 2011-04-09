/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let cw;
  let groupItem;

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

  let dragTabItem = function (tabItem) {
    let doc = cw.document.documentElement;
    let tabItem = groupItem.getChild(0);
    let container = tabItem.container;
    let aspectRange = getAspectRange();

    EventUtils.synthesizeMouseAtCenter(container, {type: "mousedown"}, cw);
    for (let x = 200; x <= 400; x += 100)
      EventUtils.synthesizeMouse(doc, x, 100, {type: "mousemove"}, cw);
    ok(aspectRange.contains(getTabItemAspect(tabItem)), "tabItem's aspect is correct");

    ok(!groupItem.getBounds().intersects(tabItem.getBounds()), "tabItem was moved out of group bounds");
    ok(!tabItem.parent, "tabItem is orphaned");

    EventUtils.synthesizeMouseAtCenter(container, {type: "mouseup"}, cw);
    ok(aspectRange.contains(getTabItemAspect(tabItem)), "tabItem's aspect is correct");

    tabItem.close();
  }

  let testDragOutOfStackedGroup = function () {
    dragTabItem();
    testDragOutOfExpandedStackedGroup();
  }

  let testDragOutOfExpandedStackedGroup = function () {
    groupItem.addSubscriber(groupItem, "expanded", function () {
      groupItem.removeSubscriber(groupItem, "expanded");

      dragTabItem();
      closeGroupItem(groupItem, function () hideTabView(finishTest));
    });

    groupItem.expand();
  }

  let finishTest = function () {
    is(cw.GroupItems.groupItems.length, 1, "there is one groupItem");
    is(gBrowser.tabs.length, 1, "there is one tab");
    ok(!TabView.isVisible(), "tabview is hidden");

    finish();
  }

  waitForExplicitFinish();

  newWindowWithTabView(function (win) {
    registerCleanupFunction(function () {
      if (!win.closed)
        win.close();
    });

    cw = win.TabView.getContentWindow();
    groupItem = createGroupItemWithBlankTabs(win, 200, 200, 10, 10);

    testDragOutOfStackedGroup();
  });
}
