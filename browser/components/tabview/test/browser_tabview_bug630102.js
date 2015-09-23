/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let win, contentWindow, originalTab, newTab1, newTab2;

  let partOne = function () {
    let groupItem = contentWindow.GroupItems.groupItems[0];
    let tabItems = groupItem.getChildren();
    is(tabItems.length, 2, "There are two tab items in that group item");
    is(tabItems[0].tab, originalTab, "The first tab item is linked to the first tab");
    is(tabItems[1].tab, newTab2, "The second tab item is linked to the second tab");

    hideTabView(partTwo, win);
  };

  let partTwo = function () {
    win.gBrowser.unpinTab(newTab1);
    showTabView(partThree, win);
  };

  let partThree = function () {
    let tabItems = contentWindow.GroupItems.groupItems[0].getChildren();
    is(tabItems.length, 3, "There are three tab items in that group item");
    is(tabItems[0].tab, win.gBrowser.tabs[0], "The first tab item is linked to the first tab");
    is(tabItems[1].tab, win.gBrowser.tabs[1], "The second tab item is linked to the second tab");
    is(tabItems[2].tab, win.gBrowser.tabs[2], "The third tab item is linked to the third tab");

    finish();
  };

  let onLoad = function (tvwin) {
    win = tvwin;
    registerCleanupFunction(() => win.close());

    for (let i = 0; i < 2; i++)
      win.gBrowser.loadOneTab("about:blank", {inBackground: true});

    [originalTab, newTab1, newTab2] = win.gBrowser.tabs;
    win.gBrowser.pinTab(newTab1);
  };

  let onShow = function () {
    contentWindow = win.TabView.getContentWindow();
    is(contentWindow.GroupItems.groupItems.length, 1, "There is only one group item");

    partOne();
  };

  waitForExplicitFinish();
  newWindowWithTabView(onShow, onLoad);
}
