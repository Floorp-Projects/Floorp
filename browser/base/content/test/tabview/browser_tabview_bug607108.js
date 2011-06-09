/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(function (win) {
    registerCleanupFunction(function () win.close());

    let cw = win.TabView.getContentWindow();
    let content = cw.document.getElementById("content");

    let groupItems = cw.GroupItems.groupItems;
    is(groupItems.length, 1, "there is one groupItem");
    groupItems[0].setSize(150, 150, true);

    waitForFocus(function () {
      // drag to create a new group
      EventUtils.synthesizeMouse(content, 200, 50, {type: "mousedown"}, cw);
      EventUtils.synthesizeMouse(content, 400, 250, {type: "mousemove"}, cw);
      EventUtils.synthesizeMouse(content, 200, 50, {type: "mouseup"}, cw);

      // enter a title for the new group
      EventUtils.synthesizeKey("t", {}, cw);
      EventUtils.synthesizeKey("VK_RETURN", {}, cw);

      is(groupItems.length, 2, "there are two groupItems");
      is(groupItems[1].getTitle(), "t", "new groupItem's title is correct");

      waitForFocus(finish);
    }, cw);
  });
}
