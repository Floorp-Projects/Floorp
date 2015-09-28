/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(function (win) {
    registerCleanupFunction(() => win.close());

    let cw = win.TabView.getContentWindow();
    let groupItem = cw.GroupItems.groupItems[0];

    // create some tabs with favIcons
    for (let i = 0; i < 3; i++)
      win.gBrowser.loadOneTab("http://mochi.test:8888/browser/browser/components/tabview/test/test_bug644097.html", {inBackground: true});

    win.gBrowser.removeTab(win.gBrowser.tabs[0]);

    // shrink the group until it stacks
    let size = 400;
    while (!groupItem.isStacked() && --size)
      groupItem.setSize(size, size, true);

    // determine the tabItem at the top of the stack
    let tabItem;
    groupItem.getChildren().forEach(function (item) {
      if (groupItem.isTopOfStack(item))
        tabItem = item;
    });

    ok(tabItem, "we found the tabItem at the top of the stack");

    let fav = tabItem.$fav;
    is(fav.css("display"), "block", "the favIcon is visible");
    is(fav.css("left"), "0px", "the favIcon is at the left-most position");
    is(fav.css("top"), "0px", "the favIcon is at the top-most position");

    finish();
  });
}
