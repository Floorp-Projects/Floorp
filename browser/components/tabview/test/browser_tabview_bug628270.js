/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let cw;
  let prefix = 'start';

  let createGroupItem = function () {
    let bounds = new cw.Rect(20, 20, 200, 200);
    let groupItem = new cw.GroupItem([], {bounds: bounds, immediately: true});

    cw.UI.setActive(groupItem);
    gBrowser.loadOneTab('http://mochi.test:8888/', {inBackground: true});
    gBrowser.loadOneTab('http://mochi.test:8888/', {inBackground: true});

    let groupItemId = groupItem.id;
    registerCleanupFunction(function() {
      let groupItem = cw.GroupItems.groupItem(groupItemId);
      if (groupItem)
        groupItem.close();
    });
  }

  let getGroupItem = function (index) {
    return cw.GroupItems.groupItems[index];
  }

  let activateFirstGroupItem = function () {
    let activeTabItem = getGroupItem(0).getChild(0);
    cw.GroupItems.updateActiveGroupItemAndTabBar(activeTabItem);
  }

  let assertTabViewIsHidden = function () {
    ok(!TabView.isVisible(), prefix + ': tabview is hidden');
  }

  let assertNumberOfGroups = function (num) {
    is(cw.GroupItems.groupItems.length, num, prefix + ': there are ' + num + ' groups');
  }

  let assertNumberOfTabs = function (num) {
    is(gBrowser.visibleTabs.length, num, prefix + ': there are ' + num + ' tabs');
  }

  let assertNumberOfPinnedTabs = function (num) {
    is(gBrowser._numPinnedTabs, num, prefix + ': there are ' + num + ' pinned tabs');
  }

  let assertNumberOfTabsInGroup = function (groupItem, num) {
    is(groupItem.getChildren().length, num, prefix + ': there are ' + num + ' tabs in the group');
  }

  let assertValidPrerequisites = function () {
    assertNumberOfTabs(1);
    assertNumberOfGroups(1);
    assertNumberOfPinnedTabs(0);
  }

  let finishTest = function () {
    prefix = 'finish';
    assertValidPrerequisites();
    assertTabViewIsHidden();
    finish();
  }

  let testRestoreTabFromInactiveGroup = function () {
    prefix = 'restore';
    activateFirstGroupItem();

    let groupItem = getGroupItem(1);
    let tabItem = groupItem.getChild(0);

    // Wait until the tab has been removed but close it ourselves.
    let promise = BrowserTestUtils.removeTab(tabItem.tab, {dontRemove: true});

    // Close the tab.
    EventUtils.synthesizeMouseAtCenter(
      tabItem.$close[0], {}, TabView.getContentWindow());

    promise.then(() => {
      assertNumberOfTabsInGroup(groupItem, 1);

      restoreTab(function () {
        assertNumberOfTabsInGroup(groupItem, 2);

        activateFirstGroupItem();
        gBrowser.removeTab(gBrowser.tabs[1]);
        gBrowser.removeTab(gBrowser.tabs[1]);
        hideTabView(finishTest);
      });
    });
  }

  waitForExplicitFinish();
  assertTabViewIsHidden();
  registerCleanupFunction(() => TabView.hide());

  showTabView(function () {
    cw = TabView.getContentWindow();
    assertValidPrerequisites();

    createGroupItem();
    afterAllTabsLoaded(testRestoreTabFromInactiveGroup);
  });
}
