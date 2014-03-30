/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let cw;

function test() {
  requestLongerTimeout(2);
  waitForExplicitFinish();

  newWindowWithTabView(function(win) {
    cw = win.TabView.getContentWindow();

    let groupItemOne = cw.GroupItems.groupItems[0];
    is(groupItemOne.getChildren().length, 1, "Group one has 1 tab item");

    let groupItemTwo = createGroupItemWithBlankTabs(win, 300, 300, 40, 2);
    is(groupItemTwo.getChildren().length, 2, "Group two has 2 tab items");

    let groupItemThree = createGroupItemWithBlankTabs(win, 300, 300, 40, 2);
    is(groupItemThree.getChildren().length, 2, "Group three has 2 tab items");

    waitForFocus(() => {
      testMoreRecentlyUsedGroup(groupItemOne, groupItemTwo, function() {
        testMoreRecentlyUsedGroup(groupItemOne, groupItemThree, function() {
          testRemoveGroupAndCheckMoreRecentlyUsedGroup(groupItemOne, groupItemTwo);
        });
      });
    }, cw);
  });
}

function testMoreRecentlyUsedGroup(groupItemOne, otherGroupItem, callback) {
  let tabItem = otherGroupItem.getChild(1);
  cw.UI.setActive(tabItem);
  is(otherGroupItem.getActiveTab(), tabItem, "The second item in the other group is active");
  is(cw.GroupItems.getActiveGroupItem(), otherGroupItem, "The other group is active");

  let tabItemInGroupItemOne = groupItemOne.getChild(0);
  cw.UI.setActive(tabItemInGroupItemOne);
  is(groupItemOne.getActiveTab(), tabItemInGroupItemOne, "The first item in group one is active");
  is(cw.GroupItems.getActiveGroupItem(), groupItemOne, "The group one is active");

  groupItemOne.addSubscriber("groupHidden", function onHide() {
    groupItemOne.removeSubscriber("groupHidden", onHide);

    // group item three should have the focus
    is(otherGroupItem.getActiveTab(), tabItem, "The second item in the other group is active after group one is hidden");
    is(cw.GroupItems.getActiveGroupItem(), otherGroupItem, "The other group is active active after group one is hidden");

    groupItemOne.addSubscriber("groupShown", function onShown() {
      groupItemOne.removeSubscriber("groupShown", onShown);

      is(groupItemOne.getActiveTab(), tabItemInGroupItemOne, "The first item in group one is active after it is shown");
      is(cw.GroupItems.getActiveGroupItem(), groupItemOne, "The group one is active after it is shown");

      callback();
    });
    // click on the undo button
    EventUtils.sendMouseEvent(
      { type: "click" }, groupItemOne.$undoContainer[0], cw);
  });
  // click on the close button of group item one
  let closeButton = groupItemOne.container.getElementsByClassName("close");
  ok(closeButton[0], "Group item one close button exists");
  EventUtils.sendMouseEvent({ type: "click" }, closeButton[0], cw);
}

function testRemoveGroupAndCheckMoreRecentlyUsedGroup(groupItemOne, groupItemTwo) {
  let tabItem = groupItemTwo.getChild(0);
  cw.UI.setActive(tabItem);

  is(groupItemTwo.getActiveTab(), tabItem, "The first item in the group two is active");
  is(cw.GroupItems.getActiveGroupItem(), groupItemTwo, "The group two is active");

  let tabItemInGroupItemOne = groupItemOne.getChild(0);

  tabItemInGroupItemOne.addSubscriber("close", function onClose() {
    tabItemInGroupItemOne.removeSubscriber("close", onClose);

    is(groupItemTwo.getActiveTab(), tabItem, "The first item in the group two is still active after group one is closed");
    is(cw.GroupItems.getActiveGroupItem(), groupItemTwo, "The group two is still active after group one is closed");

    promiseWindowClosed(cw.top).then(() => {
      cw = null;
      finish();
    });
  });
  // close the tab item and the group item
  let closeButton = tabItemInGroupItemOne.container.getElementsByClassName("close");
  ok(closeButton[0], "Tab item close button exists");
  EventUtils.sendMouseEvent({ type: "mousedown" }, closeButton[0], cw);
  EventUtils.sendMouseEvent({ type: "mouseup" }, closeButton[0], cw);
}
