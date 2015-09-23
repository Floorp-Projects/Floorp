/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that groups behave properly when closing all tabs but app tabs.

function test() {
  let cw, win, groupItem;

  let onLoad = function (tvwin) {
    win = tvwin;
    registerCleanupFunction(() => win.close());
    win.gBrowser.pinTab(win.gBrowser.tabs[0]);
    win.gBrowser.loadOneTab("about:blank", {inBackground: true});
  };

  let onShow = function () {
    cw = win.TabView.getContentWindow();
    is(cw.GroupItems.groupItems.length, 1, "There's only one group");

    groupItem = createEmptyGroupItem(cw, 200, 200, 20);
    cw.UI.setActive(groupItem);

    whenTabViewIsHidden(onHide, win);
    cw.UI.goToTab(win.gBrowser.tabs[0]);
  };

  let onHide = function () {
    let tab = win.gBrowser.loadOneTab("about:blank", {inBackground: true});
    is(groupItem.getChildren().length, 1, "One tab is in the new group");

    executeSoon(function () {
      is(win.gBrowser.visibleTabs.length, 2, "There are two tabs displayed");
      win.gBrowser.removeTab(tab);

      is(groupItem.getChildren().length, 0, "No tabs are in the new group");
      is(win.gBrowser.visibleTabs.length, 1, "There is one tab displayed");
      is(cw.GroupItems.groupItems.length, 2, "There are two groups still");

      finish();
    });
  };

  waitForExplicitFinish();

  newWindowWithTabView(onShow, onLoad);
}
