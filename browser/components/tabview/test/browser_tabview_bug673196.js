/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  function onLoad(win) {
    registerCleanupFunction(() => win.close());
    win.gBrowser.addTab();
  }

  function onShow(win) {
    let cw = win.TabView.getContentWindow();
    let group = cw.GroupItems.groupItems[0];

    // shrink the group to make some room for dragging
    group.setSize(200, 200, true);

    waitForFocus(function () {
      let target = group.getChild(0).container;
      EventUtils.synthesizeMouseAtCenter(target, {type: "mousedown"}, cw);
      EventUtils.synthesizeMouse(target, 0, 300, {type: "mousemove"}, cw);
      EventUtils.synthesizeMouseAtCenter(target, {type: "mouseup"}, cw);

      let newGroup = cw.GroupItems.groupItems[1];
      let groupBounds = newGroup.getBounds();

      let safeWindowBounds = cw.Items.getSafeWindowBounds();
      ok(safeWindowBounds.contains(groupBounds),
         "new group is within safe window bounds");

      finish();
    }, cw);
  }

  waitForExplicitFinish();
  newWindowWithTabView(onShow, onLoad);
}
