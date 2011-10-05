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
  }

  let testDragOutOfStackedGroup = function () {
    dragTabItem();

    let secondGroup = cw.GroupItems.groupItems[1];
    closeGroupItem(secondGroup, testDragOutOfExpandedStackedGroup);
  }

  let testDragOutOfExpandedStackedGroup = function () {
    groupItem.addSubscriber("expanded", function onExpanded() {
      groupItem.removeSubscriber("expanded", onExpanded);
      dragTabItem();
    });

    groupItem.addSubscriber("collapsed", function onCollapsed() {
      groupItem.removeSubscriber("collapsed", onCollapsed);

      let secondGroup = cw.GroupItems.groupItems[1];
      closeGroupItem(secondGroup, function () hideTabView(finishTest));
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
    registerCleanupFunction(function () win.close());

    cw = win.TabView.getContentWindow();

    groupItem = cw.GroupItems.groupItems[0];
    groupItem.setSize(200, 200, true);

    for (let i = 0; i < 9; i++)
      win.gBrowser.addTab();

    ok(groupItem.isStacked(), "groupItem is stacked");
    testDragOutOfStackedGroup();
  });
}
