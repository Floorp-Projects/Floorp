/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let cw;

  let createGroupItem = function () {
    let bounds = new cw.Rect(20, 20, 400, 200);
    let groupItem = new cw.GroupItem([], {bounds: bounds, immediately: true});

    let groupItemId = groupItem.id;
    registerCleanupFunction(function() {
      let groupItem = cw.GroupItems.groupItem(groupItemId);
      if (groupItem)
        groupItem.close();
    });

    return groupItem;
  }

  let assertNumberOfGroups = function (num) {
    is(cw.GroupItems.groupItems.length, num, 'there should be ' + num + ' groups');
  }

  let assertNumberOfTabs = function (num) {
    is(gBrowser.tabs.length, num, 'there should be ' + num + ' tabs');
  }

  let simulateDoubleClick = function (target, button) {
    for (let i=0; i<2; i++)
      EventUtils.synthesizeMouseAtCenter(target, {button: button || 0}, cw);
  }

  let finishTest = function () {
    let tabItem = gBrowser.tabs[0]._tabViewTabItem;
    cw.GroupItems.updateActiveGroupItemAndTabBar(tabItem);

    assertNumberOfGroups(1);
    assertNumberOfTabs(1);

    finish();
  }

  let testDoubleClick = function () {
    let groupItem = createGroupItem();
    assertNumberOfGroups(2);
    assertNumberOfTabs(1);

    // simulate double click on group title
    let input = groupItem.$title[0];
    simulateDoubleClick(input);
    assertNumberOfTabs(1);

    // simulate double click on title bar
    let titlebar = groupItem.$titlebar[0];
    simulateDoubleClick(titlebar);
    assertNumberOfTabs(1);

    // simulate double click with middle mouse button
    let container = groupItem.container;
    simulateDoubleClick(container, 1);
    assertNumberOfTabs(1);

    // simulate double click with right mouse button
    simulateDoubleClick(container, 2);
    assertNumberOfTabs(1);

    groupItem.close();
    hideTabView(finishTest);
  }

  waitForExplicitFinish();
  registerCleanupFunction(function () TabView.hide());

  showTabView(function () {
    cw = TabView.getContentWindow();
    testDoubleClick();
  });
}
