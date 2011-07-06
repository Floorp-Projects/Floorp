/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let cw;

  let createGroupItem = function () {
    let bounds = new cw.Rect(20, 20, 200, 200);
    let groupItem = new cw.GroupItem([], {bounds: bounds, immediately: true});

    cw.UI.setActive(groupItem);
    gBrowser.loadOneTab('about:blank', {inBackground: true});

    return groupItem;
  }

  let finishTest = function () {
    ok(!TabView.isVisible(), 'cleanup: tabview is hidden');
    is(gBrowser.tabs.length, 1, 'cleanup: there is one tab, only');
    is(cw.GroupItems.groupItems.length, 1, 'cleanup: there is one group, only');

    finish();
  }

  let testAddChildFromAnotherGroup = function () {
    let sourceGroup = cw.GroupItems.groupItems[0];
    let targetGroup = createGroupItem();

    afterAllTabsLoaded(function () {
      // check setup
      is(sourceGroup.getChildren().length, 1, 'setup: source group has one child');
      is(targetGroup.getChildren().length, 1, 'setup: target group has one child');

      let tabItem = sourceGroup.getChild(0);
      targetGroup.add(tabItem);

      // check state after adding tabItem to targetGroup
      is(tabItem.parent, targetGroup, 'tabItem changed groups');
      is(cw.GroupItems.groupItems.length, 1, 'source group was closed automatically');
      is(targetGroup.getChildren().length, 2, 'target group has now two children');

      // cleanup and finish
      tabItem.close();
      hideTabView(finishTest);
    });
  }

  waitForExplicitFinish();

  showTabView(function () {
    cw = TabView.getContentWindow();
    testAddChildFromAnotherGroup();
  });
}
