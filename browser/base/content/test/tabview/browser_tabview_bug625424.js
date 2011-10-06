/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let win;
  let cw;

  let getGroupItem = function (index) {
    return cw.GroupItems.groupItems[index];
  }

  let newWindow = function (test) {
    newWindowWithTabView(function (tvwin) {
      registerCleanupFunction(function () {
        if (!tvwin.closed)
          tvwin.close();
      });

      win = tvwin;
      cw = win.TabView.getContentWindow();

      // setup group items
      getGroupItem(0).setSize(200, 200, true);
      createGroupItemWithBlankTabs(win, 200, 200, 300, 1);

      test();
    });
  }

  let assertNumberOfTabsInGroupItem = function (groupItem, numTabs) {
    is(groupItem.getChildren().length, numTabs,
        'there are ' + numTabs + ' tabs in this groupItem');
  }

  let testDragOnHiddenGroup = function () {
    let groupItem = getGroupItem(1);

    hideGroupItem(groupItem, function () {
      let drag = groupItem.getChild(0).container;
      let drop = groupItem.$undoContainer[0];

      assertNumberOfTabsInGroupItem(groupItem, 1);

      EventUtils.synthesizeMouseAtCenter(drag, {type: 'mousedown'}, cw);
      EventUtils.synthesizeMouseAtCenter(drop, {type: 'mousemove'}, cw);
      EventUtils.synthesizeMouseAtCenter(drop, {type: 'mouseup'}, cw);

      assertNumberOfTabsInGroupItem(groupItem, 1);

      win.close();
      newWindow(testDragOnVisibleGroup);
    });
  }

  let testDragOnVisibleGroup = function () {
    let groupItem = getGroupItem(0);
    let drag = getGroupItem(1).getChild(0).container;
    let drop = groupItem.container;

    assertNumberOfTabsInGroupItem(groupItem, 1);

    EventUtils.synthesizeMouseAtCenter(drag, {type: 'mousedown'}, cw);
    EventUtils.synthesizeMouseAtCenter(drop, {type: 'mousemove'}, cw);
    EventUtils.synthesizeMouseAtCenter(drop, {type: 'mouseup'}, cw);

    assertNumberOfTabsInGroupItem(groupItem, 2);

    win.close();
    finish();
  }

  waitForExplicitFinish();
  newWindow(testDragOnHiddenGroup);
}
