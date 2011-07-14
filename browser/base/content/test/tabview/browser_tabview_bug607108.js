/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let cw;

  let assertNumberOfGroupItems = function (num) {
    let groupItems = cw.GroupItems.groupItems;
    is(groupItems.length, num, "number of groupItems is " + num);
  };

  let dragTabOutOfGroup = function (groupItem) {
    let tabItem = groupItem.getChild(0);
    let target = tabItem.container;

    EventUtils.synthesizeMouseAtCenter(target, {type: "mousedown"}, cw);
    EventUtils.synthesizeMouse(target, 400, 100, {type: "mousemove"}, cw);
    EventUtils.synthesizeMouseAtCenter(target, {type: "mouseup"}, cw);
  };

  let testCreateGroup = function (callback) {
    let content = cw.document.getElementById("content");

    // drag to create a new group
    EventUtils.synthesizeMouse(content, 400, 50, {type: "mousedown"}, cw);
    EventUtils.synthesizeMouse(content, 500, 250, {type: "mousemove"}, cw);
    EventUtils.synthesizeMouse(content, 500, 250, {type: "mouseup"}, cw);

    assertNumberOfGroupItems(2);

    // enter a title for the new group
    EventUtils.synthesizeKey("t", {}, cw);
    EventUtils.synthesizeKey("VK_RETURN", {}, cw);


    let groupItem = cw.GroupItems.groupItems[1];
    is(groupItem.getTitle(), "t", "new groupItem's title is correct");

    closeGroupItem(groupItem, callback);
  };

  let testDragOutOfGroup = function (callback) {
    assertNumberOfGroupItems(1);

    let groupItem = cw.GroupItems.groupItems[0];
    dragTabOutOfGroup(groupItem);
    assertNumberOfGroupItems(2);

    // enter a title for the new group
    EventUtils.synthesizeKey("t", {}, cw);
    EventUtils.synthesizeKey("VK_RETURN", {}, cw);

    groupItem = cw.GroupItems.groupItems[1];
    is(groupItem.getTitle(), "t", "new groupItem's title is correct");
    closeGroupItem(groupItem, callback);
  };

  let onLoad = function (win) {
    registerCleanupFunction(function () win.close());

    for (let i = 0; i < 2; i++)
      win.gBrowser.addTab();
  };

  let onShow = function (win) {
    cw = win.TabView.getContentWindow();
    assertNumberOfGroupItems(1);

    let groupItem = cw.GroupItems.groupItems[0];
    groupItem.setSize(200, 600, true);

    waitForFocus(function () {
      testCreateGroup(function () {
        testDragOutOfGroup(finish);
      });
    }, cw);
  };

  waitForExplicitFinish();
  newWindowWithTabView(onShow, onLoad);
}
