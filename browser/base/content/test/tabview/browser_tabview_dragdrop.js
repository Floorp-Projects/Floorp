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
  let [originalTab] = gBrowser.visibleTabs;

  // create group one and two
  let boxOne = new contentWindow.Rect(20, 20, 300, 300);
  let groupOne = new contentWindow.GroupItem([], { bounds: boxOne });
  ok(groupOne.isEmpty(), "This group is empty");

  let boxTwo = new contentWindow.Rect(20, 400, 300, 300);
  let groupTwo = new contentWindow.GroupItem([], { bounds: boxTwo });

  groupOne.addSubscriber("childAdded", function onChildAdded() {
    groupOne.removeSubscriber("childAdded", onChildAdded);
    groupTwo.newTab();
  });

  let count = 0;
  let onTabViewShown = function() {
    if (count == 2) {
      window.removeEventListener("tabviewshown", onTabViewShown, false);
      addTest(contentWindow, groupOne.id, groupTwo.id, originalTab);
    }
  };
  let onTabViewHidden = function() {
    TabView.toggle();
    if (++count == 2)
      window.removeEventListener("tabviewhidden", onTabViewHidden, false);
  };
  window.addEventListener("tabviewshown", onTabViewShown, false);
  window.addEventListener("tabviewhidden", onTabViewHidden, false);

  // open tab in group
  groupOne.newTab();
}

function addTest(contentWindow, groupOneId, groupTwoId, originalTab) {
  let groupOne = contentWindow.GroupItems.groupItem(groupOneId);
  let groupTwo = contentWindow.GroupItems.groupItem(groupTwoId);
  let groupOneTabItemCount = groupOne.getChildren().length;
  let groupTwoTabItemCount = groupTwo.getChildren().length;
  is(groupOneTabItemCount, 1, "GroupItem one has one tab");
  is(groupTwoTabItemCount, 1, "GroupItem two has one tab as well");

  let tabItem = groupOne.getChild(0);
  ok(tabItem, "The tab item exists");

  // calculate the offsets
  let groupTwoRectCenter = groupTwo.getBounds().center();
  let tabItemRectCenter = tabItem.getBounds().center();
  let offsetX =
    Math.round(groupTwoRectCenter.x - tabItemRectCenter.x);
  let offsetY =
    Math.round(groupTwoRectCenter.y - tabItemRectCenter.y);

  function endGame() {
    groupTwo.removeSubscriber("childAdded", endGame);

    is(groupOne.getChildren().length, --groupOneTabItemCount,
       "The number of children in group one is decreased by 1");
    is(groupTwo.getChildren().length, ++groupTwoTabItemCount,
       "The number of children in group two is increased by 1");
  
    let onTabViewHidden = function() {
      window.removeEventListener("tabviewhidden", onTabViewHidden, false);
      groupTwo.closeAll();
      // close undo group
      let closeButton = groupTwo.$undoContainer.find(".close");
      EventUtils.sendMouseEvent(
        { type: "click" }, closeButton[0], contentWindow);
    };
    groupTwo.addSubscriber("close", function onClose() {
      groupTwo.removeSubscriber("close", onClose);
      finish(); 
    });
    window.addEventListener("tabviewhidden", onTabViewHidden, false);
    gBrowser.selectedTab = originalTab;
  }
  groupTwo.addSubscriber("childAdded", endGame);
  
  simulateDragDrop(tabItem.container, offsetX, offsetY, contentWindow);
}

function simulateDragDrop(element, offsetX, offsetY, contentWindow) {
  let rect = element.getBoundingClientRect();
  let startX = (rect.right - rect.left)/2;
  let startY = (rect.bottom - rect.top)/2;
  let incrementX = offsetX / 2;
  let incrementY = offsetY / 2;

  EventUtils.synthesizeMouse(
    element, startX, startY, { type: "mousedown" });
  
  for (let i = 1; i <= 2; i++) {
    EventUtils.synthesizeMouse(
      element, (startX + incrementX * i), (startY + incrementY * i), 
      { type: "mousemove" });
  }

  EventUtils.synthesizeMouse(
    element, (startX + incrementX * 2), (startY + incrementY * 2), 
    { type: "mouseup" });
}
