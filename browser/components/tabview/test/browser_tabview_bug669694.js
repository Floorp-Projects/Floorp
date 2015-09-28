/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  function onLoad(win) {
    registerCleanupFunction(() => win.close());

    let tab = win.gBrowser.addTab();
    win.gBrowser.pinTab(tab);
  }

  function onShow(win) {
    let tabs = win.gBrowser.tabs;

    // zoom into normal tab
    zoomIn(tabs[1], function () {
      is(win.gBrowser.selectedTab, tabs[1], "normal tab is selected");

      // select app tab
      win.gBrowser.selectedTab = tabs[0];

      toggleTabView(win, function () {
        is(win.gBrowser.selectedTab, tabs[0], "app tab is selected");
        finish();
      });
    });
  }

  newWindowWithTabView(onShow, onLoad);
}

// ----------
function zoomIn(tab, callback) {
  whenTabViewIsHidden(function () {
    executeSoon(callback);
  }, tab.ownerDocument.defaultView);

  tab._tabViewTabItem.zoomIn();
}

// ----------
function toggleTabView(win, callback) {
  showTabView(function () {
    hideTabView(callback, win);
  }, win);
}
