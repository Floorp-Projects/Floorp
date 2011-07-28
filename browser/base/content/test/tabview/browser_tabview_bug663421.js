/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let win, cw, groupItem;

  function checkNumberOfGroupItems(num) {
    is(cw.GroupItems.groupItems.length, num, "there are " + num + " groupItems");
  }

  function next() {
    if (tests.length)
      tests.shift()();
    else
      finish();
  }

  // Empty groups should not be closed when toggling Panorama on and off.
  function test1() {
    hideTabView(function () {
      showTabView(function () {
        checkNumberOfGroupItems(2);
        next();
      }, win);
    }, win);
  }

  // Groups should not be closed when their last tab is closed outside of Panorama.
  function test2() {
    whenTabViewIsHidden(function () {
      whenTabViewIsShown(function () {
        checkNumberOfGroupItems(2);
        next();
      }, win);

      win.gBrowser.removeTab(win.gBrowser.selectedTab);
    }, win);

    groupItem.newTab();
  }

  // Groups should be closed when their last tab is closed.
  function test3() {
    whenTabViewIsHidden(function () {
      showTabView(function () {
        let tab = win.gBrowser.tabs[1];
        tab._tabViewTabItem.close();
        checkNumberOfGroupItems(1);
        next();
      }, win);
    }, win);

    win.gBrowser.addTab();
  }

  // Groups should be closed when their last tab is dragged out.
  function test4() {
    groupItem = createGroupItemWithBlankTabs(win, 200, 200, 20, 1);
    checkNumberOfGroupItems(2);

    let tab = win.gBrowser.tabs[1];
    let target = tab._tabViewTabItem.container;

    waitForFocus(function () {
      EventUtils.synthesizeMouseAtCenter(target, {type: "mousedown"}, cw);
      EventUtils.synthesizeMouse(target, 600, 5, {type: "mousemove"}, cw);
      EventUtils.synthesizeMouse(target, 600, 5, {type: "mouseup"}, cw);

      checkNumberOfGroupItems(2);
      closeGroupItem(cw.GroupItems.groupItems[1], next);
    }, win);
  }

  let tests = [test1, test2, test3, test4];

  waitForExplicitFinish();

  newWindowWithTabView(function (aWin) {
    registerCleanupFunction(function () aWin.close());

    win = aWin;
    cw = win.TabView.getContentWindow();
    groupItem = createEmptyGroupItem(cw, 200, 200, 20);

    checkNumberOfGroupItems(2);
    next();
  });
}
