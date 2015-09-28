/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(function(win) {
    registerCleanupFunction(() => win.close());

    let cw = win.TabView.getContentWindow();
    let groupItemOne = cw.GroupItems.groupItems[0];

    let groupItemTwo = createGroupItemWithBlankTabs(win, 100, 100, 40, 2);
    ok(groupItemTwo.isStacked(), "groupItem is now stacked");

    is(win.gBrowser.tabs.length, 3, "There are three tabs");

    // the focus should remain within the group after it's expanded
    groupItemTwo.addSubscriber("expanded", function onExpanded() {
      groupItemTwo.removeSubscriber("expanded", onExpanded);

      ok(groupItemTwo.expanded, "groupItemTwo is expanded");
      is(cw.UI.getActiveTab(), groupItemTwo.getChild(0),
         "The first tab item in group item two is active in expanded mode");

      EventUtils.synthesizeKey("VK_DOWN", {}, cw);
      is(cw.UI.getActiveTab(), groupItemTwo.getChild(0),
         "The first tab item is still active after pressing down key");

      // the focus should goes to other group if the down arrow is pressed
      groupItemTwo.addSubscriber("collapsed", function onExpanded() {
        groupItemTwo.removeSubscriber("collapsed", onExpanded);

        ok(!groupItemTwo.expanded, "groupItemTwo is not expanded");
        is(cw.UI.getActiveTab(), groupItemTwo.getChild(0),
           "The first tab item is active in group item two in collapsed mode");

        EventUtils.synthesizeKey("VK_DOWN", {}, cw);
        is(cw.UI.getActiveTab(), groupItemOne.getChild(0),
           "The first tab item in group item one is active after pressing down key");

        finish();
      });

      groupItemTwo.collapse();
    });

    groupItemTwo.expand();
  });
}
