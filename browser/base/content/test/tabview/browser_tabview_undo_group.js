/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  TabView.toggle();
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);
  ok(TabView.isVisible(), "Tab View is visible");

  let contentWindow = document.getElementById("tab-view").contentWindow;

  // create a group item
  let box = new contentWindow.Rect(20, 400, 300, 300);
  let groupItem = new contentWindow.GroupItem([], { bounds: box });

  // create a tab item in the new group
  let onTabViewHidden = function() {
    window.removeEventListener("tabviewhidden", onTabViewHidden, false);

    ok(!TabView.isVisible(), "Tab View is hidden because we just opened a tab");
    // show tab view
    TabView.toggle();
  };
  let onTabViewShown = function() {
    window.removeEventListener("tabviewshown", onTabViewShown, false);

    is(groupItem.getChildren().length, 1, "The new group has a tab item");
    // start the tests
    testUndoGroup(contentWindow, groupItem);
  };
  window.addEventListener("tabviewhidden", onTabViewHidden, false);
  window.addEventListener("tabviewshown", onTabViewShown, false);

  // click on the + button
  let newTabButton = groupItem.container.getElementsByClassName("newTabButton");
  ok(newTabButton[0], "New tab button exists");

  EventUtils.sendMouseEvent({ type: "click" }, newTabButton[0], contentWindow);
}

function testUndoGroup(contentWindow, groupItem) {
  groupItem.addSubscriber(groupItem, "groupHidden", function() {
    groupItem.removeSubscriber(groupItem, "groupHidden");

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

  groupItem.addSubscriber(groupItem, "groupShown", function() {
    groupItem.removeSubscriber(groupItem, "groupShown");

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
  ok(closeButton, "Group item close button exists");
  EventUtils.sendMouseEvent({ type: "click" }, closeButton[0], contentWindow);
}

function testCloseUndoGroup(contentWindow, groupItem) {
  groupItem.addSubscriber(groupItem, "groupHidden", function() {
    groupItem.removeSubscriber(groupItem, "groupHidden");

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

  groupItem.addSubscriber(groupItem, "close", function() {
    groupItem.removeSubscriber(groupItem, "close");

    let theGroupItem = contentWindow.GroupItems.groupItem(groupItem.id);
    ok(!theGroupItem, "The group item doesn't exists");

    let endGame = function() {
      window.removeEventListener("tabviewhidden", endGame, false);
      ok(!TabView.isVisible(), "Tab View is hidden");
      finish();
    };
    window.addEventListener("tabviewhidden", endGame, false);

    // after the last selected tabitem is closed, there would be not active
    // tabitem on the UI so we set the active tabitem before toggling the 
    // visibility of tabview
    let tabItems = contentWindow.TabItems.getItems();
    ok(tabItems[0], "A tab item exists");
    contentWindow.UI.setActiveTab(tabItems[0]);

    TabView.toggle();
  });

  let closeButton = groupItem.container.getElementsByClassName("close");
  ok(closeButton, "Group item close button exists");
  EventUtils.sendMouseEvent({ type: "click" }, closeButton[0], contentWindow);
}
