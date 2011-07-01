/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let newTab;

  let onLoad = function (win) {
    registerCleanupFunction(function () win.close());

    newTab = win.gBrowser.addTab();

    let popup = win.document.getElementById("context_tabViewMenuPopup");
    win.TabView.updateContextMenu(newTab, popup);
  };

  let onShow = function (win) {
    let cw = win.TabView.getContentWindow();
    is(cw.GroupItems.groupItems.length, 1, "Has only one group");

    let groupItem = cw.GroupItems.groupItems[0];
    let tabItems = groupItem.getChildren();

    let tab = tabItems[tabItems.length - 1].tab;
    is(tab, newTab, "The new tab exists in the group");

    finish();
  };

  waitForExplicitFinish();
  newWindowWithTabView(onShow, onLoad);
}
