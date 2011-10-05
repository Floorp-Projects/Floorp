/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  showTabView(function () {
    let cw = TabView.getContentWindow();

    let groupItem = createGroupItemWithBlankTabs(window, 200, 240, 20, 4)
    ok(!groupItem.isStacked(), 'groupItem is not stacked');
    cw.GroupItems.pauseArrange();

    groupItem.setSize(100, 100, true);
    groupItem.setUserSize();
    ok(!groupItem.isStacked(), 'groupItem is still not stacked');

    cw.GroupItems.resumeArrange();
    ok(groupItem.isStacked(), 'groupItem is now stacked');

    closeGroupItem(groupItem, function () hideTabView(finish));
  });
}
