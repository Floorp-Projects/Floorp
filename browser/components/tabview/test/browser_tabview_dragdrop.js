/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();
  showTabView(onTabViewShown);
}

function onTabViewShown() {
  ok(TabView.isVisible(), "Tab View is visible");

  let contentWindow = TabView.getContentWindow();
  let [originalTab] = gBrowser.visibleTabs;

  let createGroupItem = function (left, top, width, height) {
    let box = new contentWindow.Rect(left, top, width, height);
    let groupItem = new contentWindow.GroupItem([], {bounds: box, immediately: true});

    contentWindow.UI.setActive(groupItem);
    gBrowser.loadOneTab("about:blank", {inBackground: true});

    return groupItem;
  };

  // create group one and two
  let groupOne = createGroupItem(20, 20, 300, 300);
  let groupTwo = createGroupItem(20, 400, 300, 300);

  waitForFocus(function () {
    addTest(contentWindow, groupOne.id, groupTwo.id, originalTab);
  });
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

    closeGroupItem(groupOne, function () {
      closeGroupItem(groupTwo, () => hideTabView(finish));
    });
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
