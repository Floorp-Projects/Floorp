/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let newTabOne;
let originalTab;

function test() {
  waitForExplicitFinish();

  originalTab = gBrowser.visibleTabs[0];
  newTabOne = gBrowser.addTab("http://mochi.test:8888/");

  let browser = gBrowser.getBrowserForTab(newTabOne);
  let onLoad = function() {
    browser.removeEventListener("load", onLoad, true);
    
    // show the tab view
    window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
    TabView.toggle();
  }
  browser.addEventListener("load", onLoad, true);
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);
  ok(TabView.isVisible(), "Tab View is visible");

  let contentWindow = document.getElementById("tab-view").contentWindow;
  testOne(contentWindow);
}

function testOne(contentWindow) {
  onSearchEnabledAndDisabled(contentWindow, function() {
    testTwo(contentWindow); 
  });
  // press cmd/ctrl F
  EventUtils.synthesizeKey("f", { accelKey: true });
}

function testTwo(contentWindow) {
  onSearchEnabledAndDisabled(contentWindow, function() { 
    testThree(contentWindow);
  });
  // press /
  EventUtils.synthesizeKey("VK_SLASH", { type: "keydown" }, contentWindow);
}

function testThree(contentWindow) {
  let groupItem = createEmptyGroupItem(contentWindow, 200);

  let onTabViewHidden = function() {
    window.removeEventListener("tabviewhidden", onTabViewHidden, false);
    TabView.toggle();
  };
  let onTabViewShown = function() {
    window.removeEventListener("tabviewshown", onTabViewShown, false);

    is(contentWindow.UI.getActiveTab(), groupItem.getChild(0), 
       "The active tab is newly created tab item");

    let onSearchEnabled = function() {
      contentWindow.removeEventListener(
        "tabviewsearchenabled", onSearchEnabled, false);

      let searchBox = contentWindow.iQ("#searchbox");

      ok(contentWindow.document.hasFocus() && 
         contentWindow.document.activeElement == searchBox[0], 
         "The search box has focus");
      searchBox.val(newTabOne._tabViewTabItem.$tabTitle[0].innerHTML);

      contentWindow.performSearch();

      let checkSelectedTab = function() {
        window.removeEventListener("tabviewhidden", checkSelectedTab, false);
        is(newTabOne, gBrowser.selectedTab, "The search result tab is shown");
        cleanUpAndFinish(groupItem.getChild(0), contentWindow);
      };
      window.addEventListener("tabviewhidden", checkSelectedTab, false);

      // use the tabview menu (the same as pressing cmd/ctrl + e)
      document.getElementById("menu_tabview").doCommand();
   };
   contentWindow.addEventListener("tabviewsearchenabled", onSearchEnabled, false);
   EventUtils.synthesizeKey("VK_SLASH", { type: "keydown" }, contentWindow);
  };
  window.addEventListener("tabviewhidden", onTabViewHidden, false);
  window.addEventListener("tabviewshown", onTabViewShown, false);
  
  // click on the + button
  let newTabButton = groupItem.container.getElementsByClassName("newTabButton");
  ok(newTabButton[0], "New tab button exists");

  EventUtils.sendMouseEvent({ type: "click" }, newTabButton[0], contentWindow);
}

function onSearchEnabledAndDisabled(contentWindow, callback) {
  let onSearchEnabled = function() {
    contentWindow.removeEventListener(
      "tabviewsearchenabled", onSearchEnabled, false);
    contentWindow.addEventListener("tabviewsearchdisabled", onSearchDisabled, false);
    contentWindow.hideSearch();
  }
  let onSearchDisabled = function() {
    contentWindow.removeEventListener(
      "tabviewsearchdisabled", onSearchDisabled, false);
    callback();
  }
  contentWindow.addEventListener("tabviewsearchenabled", onSearchEnabled, false);
}

function cleanUpAndFinish(tabItem, contentWindow) {
  gBrowser.selectedTab = originalTab;
  gBrowser.removeTab(newTabOne);
  gBrowser.removeTab(tabItem.tab);
  
  finish();
}

function createEmptyGroupItem(contentWindow, padding) {
  let pageBounds = contentWindow.Items.getPageBounds();
  pageBounds.inset(padding, padding);

  let box = new contentWindow.Rect(pageBounds);
  box.width = 300;
  box.height = 300;

  let emptyGroupItem = new contentWindow.GroupItem([], { bounds: box });

  return emptyGroupItem;
}

