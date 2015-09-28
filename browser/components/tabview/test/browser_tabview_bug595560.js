/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var win;
var cw;

function test() {
  waitForExplicitFinish();

  let onLoad = function (tvwin) {
    win = tvwin;
    registerCleanupFunction(() => win.close());
    win.gBrowser.loadOneTab("http://mochi.test:8888/", {inBackground: true});
  };

  let onShow = function () {
    cw = win.TabView.getContentWindow();
    ok(win.TabView.isVisible(), "Tab View is visible");
    afterAllTabItemsUpdated(testOne, win);
  };

  newWindowWithTabView(onShow, onLoad);
}

function testOne() {
  hideSearchWhenSearchEnabled(testTwo);
  // press cmd/ctrl F
  EventUtils.synthesizeKey("f", {accelKey: true}, cw);
}

function testTwo() {
  hideSearchWhenSearchEnabled(testThree);
  // press /
  EventUtils.synthesizeKey("VK_SLASH", {}, cw);
}

function testThree() {
  ok(win.TabView.isVisible(), "Tab View is visible");
  // create another group with a tab.
  let groupItem = createGroupItemWithBlankTabs(win, 300, 300, 200, 1);
  is(cw.UI.getActiveTab(), groupItem.getChild(0), 
     "The active tab is newly created tab item");

  whenSearchIsEnabled(function () {
    let doc = cw.document;
    let searchBox = cw.iQ("#searchbox");
    let hasFocus = doc.hasFocus() && doc.activeElement == searchBox[0];
    ok(hasFocus, "The search box has focus");

    let tab = win.gBrowser.tabs[1];
    searchBox.val(tab._tabViewTabItem.$tabTitle[0].innerHTML);

    cw.Search.perform();

    whenTabViewIsHidden(function () {
      is(tab, win.gBrowser.selectedTab, "The search result tab is shown");
      finish()
    }, win);

    // use the tabview menu (the same as pressing cmd/ctrl + e)
    win.document.getElementById("menu_tabview").doCommand();
  }, win);
  EventUtils.synthesizeKey("VK_SLASH", {}, cw);
}

function hideSearchWhenSearchEnabled(callback) {
  whenSearchIsEnabled(function() {
    hideSearch(callback, win);
  }, win);
}

