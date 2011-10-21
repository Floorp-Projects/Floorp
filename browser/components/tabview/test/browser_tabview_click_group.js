/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let contentWindow;
let groupItem;
let groupItemId;

function test() {
  waitForExplicitFinish();

  registerCleanupFunction(function() {
    contentWindow.gPrefBranch.clearUserPref("animate_zoom");
    let createdGroupItem = contentWindow.GroupItems.groupItem(groupItemId)
    if (createdGroupItem)
      closeGroupItem(createdGroupItem);
    hideTabView();
  });

  showTabView(function() {
    contentWindow = TabView.getContentWindow();
    groupItem = createEmptyGroupItem(contentWindow, 300, 300, 200);
    groupItemId = groupItem.id;
    testMouseClickOnEmptyGroupItem();
  });
}

function testMouseClickOnEmptyGroupItem() {
  whenTabViewIsHidden(function() {
    is(groupItem.getChildren().length, 1, "The group item contains one tab item now");
    showTabView(testDraggingWithinGroupItem);
  });
  is(groupItem.getChildren().length, 0, "The group item doesn't contain any tab items");
  EventUtils.sendMouseEvent({ type: "mousedown" }, groupItem.container, contentWindow);
  EventUtils.sendMouseEvent({ type: "mouseup" }, groupItem.container, contentWindow);
}

function testDraggingWithinGroupItem() {
  let target = groupItem.container;
  contentWindow.gPrefBranch.setBoolPref("animate_zoom", false);

  // stimulate drag and drop
  EventUtils.sendMouseEvent( {type: "mousedown" }, target, contentWindow);
  EventUtils.synthesizeMouse(target, 10, 10, { type: "mousemove" }, contentWindow);
  ok(groupItem.isDragging, "The group item is being dragged")

  EventUtils.sendMouseEvent({ type: "mouseup" }, target, contentWindow);
  ok(!groupItem.isDragging, "The dragging is competely");

  executeSoon(function() {
    ok(TabView.isVisible(), "The tab view is still visible after dragging");
    contentWindow.gPrefBranch.clearUserPref("animate_zoom");

    testMouseClickOnGroupItem();
  });
}

function testMouseClickOnGroupItem() {
  whenTabViewIsHidden(function() {
    is(groupItem.getChildren().length, 1, "The group item still contains one tab item");

    closeGroupItem(groupItem, function() {
      hideTabView(finish);
    });
  });
  EventUtils.sendMouseEvent({ type: "mousedown" }, groupItem.container, contentWindow);
  EventUtils.sendMouseEvent({ type: "mouseup" }, groupItem.container, contentWindow);
}

