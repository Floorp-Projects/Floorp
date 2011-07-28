/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  showTabView(onTabViewWindowLoaded);
}

let originalGroupItem = null;
let originalTab = null;

function onTabViewWindowLoaded() {
  ok(TabView.isVisible(), "Tab View is visible");

  let contentWindow = TabView.getContentWindow();

  is(contentWindow.GroupItems.groupItems.length, 1, "There is one group item on startup");
  originalGroupItem = contentWindow.GroupItems.groupItems[0];
  is(originalGroupItem.getChildren().length, 1, "There should be one Tab Item in that group.");
  contentWindow.UI.setActive(originalGroupItem);

  [originalTab] = gBrowser.visibleTabs;

  testEmptyGroupItem(contentWindow);
}

function testEmptyGroupItem(contentWindow) {
  let groupItemCount = contentWindow.GroupItems.groupItems.length;
  
  // create empty group item
  let emptyGroupItem = createEmptyGroupItem(contentWindow, 300, 300, 100);
  ok(emptyGroupItem.isEmpty(), "This group is empty");

  is(contentWindow.GroupItems.groupItems.length, ++groupItemCount,
     "The number of groups is increased by 1");

  emptyGroupItem.addSubscriber("close", function onClose() {
    emptyGroupItem.removeSubscriber("close", onClose);

    // check the number of groups.
    is(contentWindow.GroupItems.groupItems.length, --groupItemCount,
       "The number of groups is decreased by 1");

    testGroupItemWithTabItem(contentWindow);
  });

  let closeButton = emptyGroupItem.container.getElementsByClassName("close");
  ok(closeButton[0], "Group close button exists");

  // click the close button
  EventUtils.synthesizeMouse(closeButton[0], 1, 1, {}, contentWindow);
}

function testGroupItemWithTabItem(contentWindow) {
  let groupItem = createEmptyGroupItem(contentWindow, 300, 300, 200);
  let tabItemCount = 0;

  let onTabViewShown = function() {
    let tabItem = groupItem.getChild(groupItem.getChildren().length - 1);
    ok(tabItem, "Tab item exists");

    let tabItemClosed = false;
    tabItem.addSubscriber("close", function onClose() {
      tabItem.removeSubscriber("close", onClose);
      tabItemClosed = true;
    });
    tabItem.addSubscriber("tabRemoved", function onTabRemoved() {
      tabItem.removeSubscriber("tabRemoved", onTabRemoved);

      ok(tabItemClosed, "The tab item is closed");
      is(groupItem.getChildren().length, --tabItemCount,
        "The number of children in new tab group is decreased by 1");

      ok(TabView.isVisible(), "Tab View is still shown");

      // Now there should only be one tab left, so we need to hide TabView
      // and go into that tab.
      is(gBrowser.tabs.length, 1, "There is only one tab left");

      // after the last selected tabitem is closed, there would be not active
      // tabitem on the UI so we set the active tabitem before toggling the 
      // visibility of tabview
      let tabItems = contentWindow.TabItems.getItems();
      ok(tabItems[0], "A tab item exists");
      contentWindow.UI.setActive(tabItems[0]);

      hideTabView(function() {
        ok(!TabView.isVisible(), "Tab View is hidden");

        closeGroupItem(groupItem, finish);
      });
    });

    // remove the tab item.  The code detects mousedown and mouseup so we stimulate here
    let closeButton = tabItem.container.getElementsByClassName("close");
    ok(closeButton, "Tab item close button exists");

    EventUtils.sendMouseEvent({ type: "mousedown" }, closeButton[0], contentWindow);
    EventUtils.sendMouseEvent({ type: "mouseup" }, closeButton[0], contentWindow);
  };

  whenTabViewIsHidden(function() {
    is(groupItem.getChildren().length, ++tabItemCount,
       "The number of children in new tab group is increased by 1");

    ok(!TabView.isVisible(), "Tab View is hidden because we just opened a tab");
    showTabView(onTabViewShown);
  });
  groupItem.newTab();
}
