/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  registerCleanupFunction(function() {
    while (gBrowser.tabs[1])
      gBrowser.removeTab(gBrowser.tabs[1]);
    hideTabView();
  });
  showTabView(onTabViewWindowLoaded);
}

function onTabViewWindowLoaded() {
  ok(TabView.isVisible(), "Tab View is visible");

  let contentWindow = TabView.getContentWindow();

  registerCleanupFunction(function() {
    let groupItem = contentWindow.GroupItems.groupItem(groupItemId);
    if (groupItem)
      closeGroupItem(groupItem);
  });

  // create a group item
  let groupItem = createGroupItemWithBlankTabs(window, 300, 300, 400, 1);
  let groupItemId = groupItem.id;
  is(groupItem.getChildren().length, 1, "The new group has a tab item");
  // start the tests
  waitForFocus(function() {
    testUndoGroup(contentWindow, groupItem);
  }, contentWindow);
}

function testUndoGroup(contentWindow, groupItem) {
  groupItem.addSubscriber("groupHidden", function onHidden() {
    groupItem.removeSubscriber("groupHidden", onHidden);

    // check the data of the group
    let theGroupItem = contentWindow.GroupItems.groupItem(groupItem.id);
    ok(theGroupItem, "The group item still exists");
    is(theGroupItem.getChildren().length, 1, 
       "The tab item in the group still exists");

    // check the visibility of the group element and undo element
    is(theGroupItem.container.style.display, "none", 
       "The group element is hidden");
    ok(theGroupItem.$undoContainer, "Undo container is avaliable");

    EventUtils.sendMouseEvent(
      { type: "click" }, theGroupItem.$undoContainer[0], contentWindow);
  });

  groupItem.addSubscriber("groupShown", function onShown() {
    groupItem.removeSubscriber("groupShown", onShown);

    // check the data of the group
    let theGroupItem = contentWindow.GroupItems.groupItem(groupItem.id);
    ok(theGroupItem, "The group item still exists");
    is(theGroupItem.getChildren().length, 1, 
       "The tab item in the group still exists");

    // check the visibility of the group element and undo element
    is(theGroupItem.container.style.display, "", "The group element is visible");
    ok(!theGroupItem.$undoContainer, "Undo container is not avaliable");
    
    // start the next test
    testCloseUndoGroup(contentWindow, groupItem);
  });

  let closeButton = groupItem.container.getElementsByClassName("close");
  ok(closeButton[0], "Group item close button exists");
  EventUtils.sendMouseEvent({ type: "click" }, closeButton[0], contentWindow);
}

function testCloseUndoGroup(contentWindow, groupItem) {
  groupItem.addSubscriber("groupHidden", function onHidden() {
    groupItem.removeSubscriber("groupHidden", onHidden);

    // check the data of the group
    let theGroupItem = contentWindow.GroupItems.groupItem(groupItem.id);
    ok(theGroupItem, "The group item still exists");
    is(theGroupItem.getChildren().length, 1, 
       "The tab item in the group still exists");

    // check the visibility of the group element and undo element
    is(theGroupItem.container.style.display, "none", 
       "The group element is hidden");
    ok(theGroupItem.$undoContainer, "Undo container is avaliable");

    // click on close
    let closeButton = theGroupItem.$undoContainer.find(".close");
    EventUtils.sendMouseEvent(
      { type: "click" }, closeButton[0], contentWindow);
  });

  groupItem.addSubscriber("close", function onClose() {
    groupItem.removeSubscriber("close", onClose);

    let theGroupItem = contentWindow.GroupItems.groupItem(groupItem.id);
    ok(!theGroupItem, "The group item doesn't exists");

    // after the last selected tabitem is closed, there would be not active
    // tabitem on the UI so we set the active tabitem before toggling the 
    // visibility of tabview
    let tabItems = contentWindow.TabItems.getItems();
    ok(tabItems[0], "A tab item exists");
    contentWindow.UI.setActive(tabItems[0]);

    hideTabView(function() {
      ok(!TabView.isVisible(), "Tab View is hidden");
      finish();
    });
  });

  let closeButton = groupItem.container.getElementsByClassName("close");
  ok(closeButton[0], "Group item close button exists");
  EventUtils.sendMouseEvent({ type: "click" }, closeButton[0], contentWindow);
}
