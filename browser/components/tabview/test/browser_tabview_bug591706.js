/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  if (TabView.isVisible())
    onTabViewWindowLoaded();
  else
    TabView.show();
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);

  ok(TabView.isVisible(), "Tab View is visible");

  let contentWindow = document.getElementById("tab-view").contentWindow;
  let [originalTab] = gBrowser.visibleTabs;

  // Create a first tab and orphan it
  let firstTab = gBrowser.loadOneTab("about:blank#1", {inBackground: true});
  let firstTabItem = firstTab._tabViewTabItem;
  let currentGroup = contentWindow.GroupItems.getActiveGroupItem();
  ok(currentGroup.getChildren().some(child => child == firstTabItem),"The first tab was made in the current group");
  contentWindow.GroupItems.getActiveGroupItem().remove(firstTabItem);
  ok(!currentGroup.getChildren().some(child => child == firstTabItem),"The first tab was orphaned");

  // Create a group and make it active
  let box = new contentWindow.Rect(10, 10, 300, 300);
  let group = new contentWindow.GroupItem([], { bounds: box });
  ok(group.isEmpty(), "This group is empty");
  contentWindow.UI.setActive(group);
  
  // Create a second tab in this new group
  let secondTab = gBrowser.loadOneTab("about:blank#2", {inBackground: true});
  let secondTabItem = secondTab._tabViewTabItem;
  ok(group.getChildren().some(child => child == secondTabItem),"The second tab was made in our new group");
  is(group.getChildren().length, 1, "Only one tab in the first group");
  isnot(firstTab.linkedBrowser.currentURI.spec, secondTab.linkedBrowser.currentURI.spec, "The two tabs must have different locations");

  // Add the first tab to the group *programmatically*, without specifying a dropPos
  group.add(firstTabItem);
  is(group.getChildren().length, 2, "Two tabs in the group");

  is(group.getChildren()[0].tab.linkedBrowser.currentURI.spec, secondTab.linkedBrowser.currentURI.spec, "The second tab was there first");
  is(group.getChildren()[1].tab.linkedBrowser.currentURI.spec, firstTab.linkedBrowser.currentURI.spec, "The first tab was just added and went to the end of the line");

  group.addSubscriber("close", function onClose() {
    group.removeSubscriber("close", onClose);

    ok(group.isEmpty(), "The group is empty again");

    is(contentWindow.GroupItems.getActiveGroupItem(), currentGroup, "There is an active group");
    is(gBrowser.tabs.length, 1, "There is only one tab left");
    is(gBrowser.visibleTabs.length, 1, "There is also only one visible tab");

    let onTabViewHidden = function() {
      window.removeEventListener("tabviewhidden", onTabViewHidden, false);
      finish();
    };
    window.addEventListener("tabviewhidden", onTabViewHidden, false);
    gBrowser.selectedTab = originalTab;

    TabView.hide();
  });

  // Get rid of the group and its children
  group.closeAll();
  // close undo group
  let closeButton = group.$undoContainer.find(".close");
  EventUtils.sendMouseEvent(
    { type: "click" }, closeButton[0], contentWindow);
}

function simulateDragDrop(srcElement, offsetX, offsetY, contentWindow) {
  // enter drag mode
  let dataTransfer;

  EventUtils.synthesizeMouse(
    srcElement, 1, 1, { type: "mousedown" }, contentWindow);
  event = contentWindow.document.createEvent("DragEvents");
  event.initDragEvent(
    "dragenter", true, true, contentWindow, 0, 0, 0, 0, 0,
    false, false, false, false, 1, null, dataTransfer);
  srcElement.dispatchEvent(event);

  // drag over
  for (let i = 4; i >= 0; i--)
    EventUtils.synthesizeMouse(
      srcElement,  Math.round(offsetX/5),  Math.round(offsetY/4),
      { type: "mousemove" }, contentWindow);
  event = contentWindow.document.createEvent("DragEvents");
  event.initDragEvent(
    "dragover", true, true, contentWindow, 0, 0, 0, 0, 0,
    false, false, false, false, 0, null, dataTransfer);
  srcElement.dispatchEvent(event);

  // drop
  EventUtils.synthesizeMouse(srcElement, 0, 0, { type: "mouseup" }, contentWindow);
  event = contentWindow.document.createEvent("DragEvents");
  event.initDragEvent(
    "drop", true, true, contentWindow, 0, 0, 0, 0, 0,
    false, false, false, false, 0, null, dataTransfer);
  srcElement.dispatchEvent(event);
}
