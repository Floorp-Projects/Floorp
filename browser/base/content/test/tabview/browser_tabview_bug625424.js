/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let win;
  let cw;

  let getGroupItem = function (index) {
    return cw.GroupItems.groupItems[index];
  }

  let createOrphan = function (callback) {
    let tab = win.gBrowser.loadOneTab('about:blank', {inBackground: true});
    afterAllTabsLoaded(function () {
      let tabItem = tab._tabViewTabItem;
      tabItem.parent.remove(tabItem);
      callback(tabItem);
    });
  }

  let hideGroupItem = function (groupItem, callback) {
    groupItem.addSubscriber(groupItem, 'groupHidden', function () {
      groupItem.removeSubscriber(groupItem, 'groupHidden');
      callback();
    });
    groupItem.closeAll();
  }

  let newWindow = function (test) {
    newWindowWithTabView(function (tvwin) {
      registerCleanupFunction(function () {
        if (!tvwin.closed)
          tvwin.close();
      });

      win = tvwin;
      cw = win.TabView.getContentWindow();
      test();
    });
  }

  let assertNumberOfTabsInGroupItem = function (groupItem, numTabs) {
    is(groupItem.getChildren().length, numTabs,
        'there are ' + numTabs + ' tabs in this groupItem');
  }

  let testDragOnHiddenGroup = function () {
    createOrphan(function (orphan) {
      let groupItem = getGroupItem(0);
      hideGroupItem(groupItem, function () {
        let drag = orphan.container;
        let drop = groupItem.$undoContainer[0];

        assertNumberOfTabsInGroupItem(groupItem, 1);

        EventUtils.synthesizeMouseAtCenter(drag, {type: 'mousedown'}, cw);
        EventUtils.synthesizeMouseAtCenter(drop, {type: 'mousemove'}, cw);
        EventUtils.synthesizeMouseAtCenter(drop, {type: 'mouseup'}, cw);

        assertNumberOfTabsInGroupItem(groupItem, 1);

        win.close();
        newWindow(testDragOnVisibleGroup);
      });
    });
  }

  let testDragOnVisibleGroup = function () {
    createOrphan(function (orphan) {
      let groupItem = getGroupItem(0);
      let drag = orphan.container;
      let drop = groupItem.container;

      assertNumberOfTabsInGroupItem(groupItem, 1);

      EventUtils.synthesizeMouseAtCenter(drag, {type: 'mousedown'}, cw);
      EventUtils.synthesizeMouseAtCenter(drop, {type: 'mousemove'}, cw);
      EventUtils.synthesizeMouseAtCenter(drop, {type: 'mouseup'}, cw);

      assertNumberOfTabsInGroupItem(groupItem, 2);

      win.close();
      finish();
    });
  }

  waitForExplicitFinish();
  newWindow(testDragOnHiddenGroup);
}
