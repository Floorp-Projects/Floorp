/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let cw;
  let win;
  let currentTest;

  let getGroupItem = function (index) {
    return cw.GroupItems.groupItems[index];
  }

  let createGroupItem = function (numTabs) {
    let bounds = new cw.Rect(20, 20, 200, 200);
    let groupItem = new cw.GroupItem([], {bounds: bounds, immediately: true});
    cw.UI.setActive(groupItem);

    for (let i=0; i<numTabs || 0; i++)
      win.gBrowser.loadOneTab('about:blank', {inBackground: true});

    return groupItem;
  }

  let tests = [];

  let next = function () {
    let test = tests.shift();

    if (test) {
      // check that the previous test left things as expected
      if (currentTest) {
        currentTest += ' (post-check)';
        assertTabViewIsHidden();
        assertNumberOfGroupItems(1);
        assertNumberOfTabs(1);
      }

      currentTest = test.name;
      showTabView(test.func, win);
    } else
      promiseWindowClosed(win).then(finish);
  }

  let assertTabViewIsHidden = function () {
    ok(!win.TabView.isVisible(), currentTest + ': tabview is hidden');
  }

  let assertNumberOfGroupItems = function (num) {
    is(cw.GroupItems.groupItems.length, num, currentTest + ': number of groupItems is equal to ' + num);
  }

  let assertNumberOfTabs = function (num) {
    is(win.gBrowser.tabs.length, num, currentTest + ': number of tabs is equal to ' + num);
  }

  let assertGroupItemRemoved = function (groupItem) {
    is(cw.GroupItems.groupItems.indexOf(groupItem), -1, currentTest + ': groupItem was removed');
  }

  let assertGroupItemExists = function (groupItem) {
    isnot(cw.GroupItems.groupItems.indexOf(groupItem), -1, currentTest + ': groupItem still exists');
  }

  // setup: 1 non-empty group
  // action: close the group
  // expected: new group with blank tab is created and zoomed into
  let testSingleGroup1 = function () {
    let groupItem = getGroupItem(0);
    closeGroupItem(groupItem, function () {
      assertNumberOfGroupItems(1);
      assertGroupItemRemoved(groupItem);
      whenTabViewIsHidden(next, win);
    });
  }

  // setup: 1 non-empty group
  // action: hide the group, exit panorama
  // expected: new group with blank tab is created and zoomed into
  let testSingleGroup2 = function () {
    let groupItem = getGroupItem(0);
    hideGroupItem(groupItem, function () {
      hideTabView(function () {
        assertNumberOfGroupItems(1);
        assertGroupItemRemoved(groupItem);
        next();
      }, win);
    });
  }

  // setup: 2 non-empty groups
  // action: close one group
  // expected: nothing should happen
  let testNonEmptyGroup1 = function () {
    let groupItem = getGroupItem(0);
    let newGroupItem = createGroupItem(1);
    assertNumberOfGroupItems(2);

    closeGroupItem(groupItem, function () {
      assertNumberOfGroupItems(1);
      assertGroupItemExists(newGroupItem);
      hideTabView(next, win);
    });
  }

  // setup: 2 non-empty groups
  // action: hide one group, exit panorama
  // expected: nothing should happen
  let testNonEmptyGroup2 = function () {
    let groupItem = getGroupItem(0);
    let newGroupItem = createGroupItem(1);
    assertNumberOfGroupItems(2);

    hideGroupItem(groupItem, function () {
      hideTabView(function () {
        assertNumberOfGroupItems(1);
        assertGroupItemRemoved(groupItem);
        assertGroupItemExists(newGroupItem);
        next();
      }, win);
    });
  }

  // setup: 1 pinned tab, 1 empty group
  // action: exit panorama
  // expected: nothing should happen
  let testPinnedTab1 = function () {
    win.gBrowser.pinTab(win.gBrowser.selectedTab);

    let groupItem = getGroupItem(0);
    hideTabView(function () {
      assertNumberOfGroupItems(1);
      assertGroupItemExists(groupItem);
      win.gBrowser.unpinTab(win.gBrowser.selectedTab);
      next();
    }, win);
  }

  // setup: 1 pinned tab
  // action: exit panorama
  // expected: new blank group is created
  let testPinnedTab2 = function () {
    win.gBrowser.pinTab(win.gBrowser.selectedTab);
    getGroupItem(0).close();

    hideTabView(function () {
      assertNumberOfTabs(1);
      assertNumberOfGroupItems(1);
      win.gBrowser.unpinTab(win.gBrowser.selectedTab);
      next();
    }, win);
  }

  // setup: 1 pinned tab, 1 empty group, 1 non-empty group
  // action: close non-empty group
  // expected: nothing should happen
  let testPinnedTab3 = function () {
    win.gBrowser.pinTab(win.gBrowser.selectedTab);

    let groupItem = getGroupItem(0);
    let newGroupItem = createGroupItem(1);
    assertNumberOfGroupItems(2);

    closeGroupItem(newGroupItem, function () {
      assertNumberOfGroupItems(1);
      assertGroupItemExists(groupItem);

      win.gBrowser.unpinTab(win.gBrowser.selectedTab);
      hideTabView(next, win);
    });
  }

  // setup: 1 pinned tab, 1 empty group, 1 non-empty group
  // action: hide non-empty group, exit panorama
  // expected: nothing should happen
  let testPinnedTab4 = function () {
    win.gBrowser.pinTab(win.gBrowser.selectedTab);

    let groupItem = getGroupItem(0);
    let newGroupItem = createGroupItem(1);
    assertNumberOfGroupItems(2);

    hideGroupItem(newGroupItem, function () {
      hideTabView(function () {
        assertNumberOfGroupItems(1);
        assertGroupItemExists(groupItem);
        assertGroupItemRemoved(newGroupItem);
        win.gBrowser.unpinTab(win.gBrowser.selectedTab);
        next();
      }, win);
    });
  }

  // setup: 1 non-empty group, 1 empty group
  // action: close non-empty group
  // expected: empty group is re-used, new tab is created and zoomed into
  let testEmptyGroup1 = function () {
    let groupItem = getGroupItem(0);
    let newGroupItem = createGroupItem(0);
    assertNumberOfGroupItems(2);

    closeGroupItem(groupItem, function () {
      assertNumberOfGroupItems(1);
      assertGroupItemExists(newGroupItem);
      whenTabViewIsHidden(next, win);
    });
  }

  // setup: 1 non-empty group, 1 empty group
  // action: hide non-empty group, exit panorama
  // expected: empty group is re-used, new tab is created and zoomed into
  let testEmptyGroup2 = function () {
    let groupItem = getGroupItem(0);
    let newGroupItem = createGroupItem(0);
    assertNumberOfGroupItems(2);

    hideGroupItem(groupItem, function () {
      hideTabView(function () {
        assertNumberOfGroupItems(1);
        assertGroupItemRemoved(groupItem);
        assertGroupItemExists(newGroupItem);
        next();
      }, win);
    });
  }

  // setup: 1 hidden group, 1 non-empty group
  // action: close non-empty group
  // expected: nothing should happen
  let testHiddenGroup1 = function () {
    let groupItem = getGroupItem(0);
    let hiddenGroupItem = createGroupItem(1);
    assertNumberOfGroupItems(2);

    hideGroupItem(hiddenGroupItem, function () {
      closeGroupItem(groupItem, function () {
        assertNumberOfGroupItems(1);
        assertGroupItemRemoved(groupItem);
        assertGroupItemExists(hiddenGroupItem);
        hideTabView(next, win);
      });
    });
  }

  // setup: 1 hidden group, 1 non-empty group
  // action: hide non-empty group, exit panorama
  // expected: new group with blank tab is created and zoomed into
  let testHiddenGroup2 = function () {
    let groupItem = getGroupItem(0);
    let hiddenGroupItem = createGroupItem(1);
    assertNumberOfGroupItems(2);

    hideGroupItem(hiddenGroupItem, function () {
      hideGroupItem(groupItem, function () {
        hideTabView(function () {
          assertNumberOfGroupItems(1);
          assertGroupItemRemoved(groupItem);
          assertGroupItemRemoved(hiddenGroupItem);
          next();
        }, win);
      });
    });
  }

  tests.push({name: 'testSingleGroup1', func: testSingleGroup1});
  tests.push({name: 'testSingleGroup2', func: testSingleGroup2});

  tests.push({name: 'testNonEmptyGroup1', func: testNonEmptyGroup1});
  tests.push({name: 'testNonEmptyGroup2', func: testNonEmptyGroup2});

  tests.push({name: 'testPinnedTab1', func: testPinnedTab1});
  tests.push({name: 'testPinnedTab2', func: testPinnedTab2});
  tests.push({name: 'testPinnedTab3', func: testPinnedTab3});
  tests.push({name: 'testPinnedTab4', func: testPinnedTab4});

  tests.push({name: 'testEmptyGroup1', func: testEmptyGroup1});
  tests.push({name: 'testEmptyGroup2', func: testEmptyGroup2});

  tests.push({name: 'testHiddenGroup1', func: testHiddenGroup1});
  tests.push({name: 'testHiddenGroup2', func: testHiddenGroup2}),

  waitForExplicitFinish();

  newWindowWithTabView(window => {
    win = window;
    cw = win.TabView.getContentWindow();
    next();
  });
}
