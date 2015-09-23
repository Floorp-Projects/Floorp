/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let cw;

  let createGroupItem = function () {
    return createGroupItemWithBlankTabs(window, 400, 200, 0, 5);
  }

  let assertCorrectItemOrder = function (items) {
    for (let i=1; i<items.length; i++) {
      if (items[i-1].tab._tPos > items[i].tab._tPos) {
        ok(false, 'tabs were correctly reordered');
        return;
      }
    }
    ok(true, 'tabs were correctly reordered');
  }

  let testVariousTabOrders = function () {
    let groupItem = createGroupItem();
    let [tab1, tab2, tab3, tab4, tab5] = groupItem.getChildren();

    // prepare tests
    let tests = [];
    tests.push([tab1, tab2, tab3, tab4, tab5]);
    tests.push([tab5, tab4, tab3, tab2, tab1]);
    tests.push([tab1, tab2, tab3, tab4]);
    tests.push([tab4, tab3, tab2, tab1]);
    tests.push([tab1, tab2, tab3]);
    tests.push([tab1, tab2, tab3]);
    tests.push([tab1, tab3, tab2]);
    tests.push([tab2, tab1, tab3]);
    tests.push([tab2, tab3, tab1]);
    tests.push([tab3, tab1, tab2]);
    tests.push([tab3, tab2, tab1]);
    tests.push([tab1, tab2]);
    tests.push([tab2, tab1]);
    tests.push([tab1]);

    // test reordering of empty groups - removes the last tab and causes
    // the groupItem to close
    tests.push([]);

    while (tests.length) {
      let test = tests.shift();

      // prepare
      let items = groupItem.getChildren();
      while (test.length < items.length)
        items[items.length-1].close();

      let orig = cw.Utils.copy(items);
      items.sort((a, b) => test.indexOf(a) - test.indexOf(b));

      // order and check
      groupItem.reorderTabsBasedOnTabItemOrder();
      assertCorrectItemOrder(items);

      // revert to original item order
      items.sort((a, b) => orig.indexOf(a) - orig.indexOf(b));
      groupItem.reorderTabsBasedOnTabItemOrder();
    }

    testMoveBetweenGroups();
  }

  let testMoveBetweenGroups = function () {
    let groupItem = createGroupItem();
    let groupItem2 = createGroupItem();
    
    afterAllTabsLoaded(function () {
      // move item from group1 to group2
      let child = groupItem.getChild(2);
      groupItem.remove(child);

      groupItem2.add(child, {index: 3});
      groupItem2.reorderTabsBasedOnTabItemOrder();

      assertCorrectItemOrder(groupItem.getChildren());
      assertCorrectItemOrder(groupItem2.getChildren());

      // move item from group2 to group1
      child = groupItem2.getChild(1);
      groupItem2.remove(child);

      groupItem.add(child, {index: 1});
      groupItem.reorderTabsBasedOnTabItemOrder();

      assertCorrectItemOrder(groupItem.getChildren());
      assertCorrectItemOrder(groupItem2.getChildren());

      // cleanup
      closeGroupItem(groupItem, function () {
        closeGroupItem(groupItem2, function () {
          hideTabView(finish);
        });
      });
    });
  }

  waitForExplicitFinish();

  showTabView(function () {
    cw = TabView.getContentWindow();
    afterAllTabsLoaded(testVariousTabOrders);
  });
}
