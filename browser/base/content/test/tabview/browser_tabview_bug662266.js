/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(function (win) {
    registerCleanupFunction(function () win.close());

    for (let i = 0; i < 4; i++)
      win.gBrowser.loadOneTab("about:blank", {inBackground: true});

    let cw = win.TabView.getContentWindow();
    let groupItem = cw.GroupItems.groupItems[0];
    let children = groupItem.getChildren();

    groupItem.setSize(200, 200, true);
    ok(groupItem.isStacked(), "groupItem is now stacked");
    is(win.gBrowser.tabs.length, 5, "we have five tabs");

    groupItem.addSubscriber("expanded", function onExpanded() {
      groupItem.removeSubscriber("expanded", onExpanded);

      ok(groupItem.expanded, "groupItem is expanded");
      let bounds = children[1].getBounds();

      // remove two tabs and see if the remaining tabs are re-arranged to fill
      // the resulting gaps
      for (let i = 0; i < 2; i++) {
        EventUtils.synthesizeMouseAtCenter(children[1].container, {button: 1}, cw);
        ok(bounds.equals(children[1].getBounds()), "tabItems were re-arranged");
      }

      waitForFocus(finish);
    });

    groupItem.expand();
  });
}
