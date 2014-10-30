/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let contentWindow;

function test() {
  requestLongerTimeout(2);
  waitForExplicitFinish();

  showTabView(test1);
}

function test1() {
  ok(TabView.isVisible(), "Tab View is visible");

  contentWindow = document.getElementById("tab-view").contentWindow;
  whenTabViewIsHidden(function() {
    ok(!TabView.isVisible(), "Tab View is not visible");
    showTabView(test2);
  });
  EventUtils.synthesizeKey("e", { accelKey: true, shiftKey: true }, contentWindow);
}

function test2() {
  ok(TabView.isVisible(), "Tab View is visible");

  whenSearchIsEnabled(function() {
    ok(contentWindow.Search.isEnabled(), "The search is enabled")

    whenSearchIsDisabled(test3);
    hideSearch();
  });
  EventUtils.synthesizeKey("f", { accelKey: true }, contentWindow);
}

function test3() {
  ok(!contentWindow.Search.isEnabled(), "The search is disabled")

  is(gBrowser.tabs.length, 1, "There is one tab before cmd/ctrl + t is pressed");

  whenTabViewIsHidden(function() { 
    is(gBrowser.tabs.length, 2, "There are two tabs after cmd/ctrl + t is pressed");

    gBrowser.tabs[0].linkedBrowser.loadURI("about:mozilla");
    gBrowser.tabs[1].linkedBrowser.loadURI("http://example.com/");

    afterAllTabsLoaded(function () {
      showTabView(test4);
    });
  });
  EventUtils.synthesizeKey("t", { accelKey: true }, contentWindow);
}

function test4() {
  is(gBrowser.tabs.length, 2, "There are two tabs");

  let onTabClose = function() {
    gBrowser.tabContainer.removeEventListener("TabClose", onTabClose, true);
    executeSoon(function() {
      is(gBrowser.tabs.length, 1, "There is one tab after removing one");

      EventUtils.synthesizeKey("t", { accelKey: true, shiftKey: true }, contentWindow);
      is(gBrowser.tabs.length, 2, "There are two tabs after restoring one");

      gBrowser.tabs[1].linkedBrowser.addEventListener("load", function listener() {
        gBrowser.tabs[1].linkedBrowser.removeEventListener("load", listener, true);

        gBrowser.tabs[0].linkedBrowser.loadURI("about:blank");
        gBrowser.selectedTab = gBrowser.tabs[0];
        test8();
      }, true);
    });
  };
  gBrowser.tabContainer.addEventListener("TabClose", onTabClose, true);
  gBrowser.removeTab(gBrowser.tabs[1]);
}

// below key combination shouldn't trigger actions in tabview UI
function test8() {
  showTabView(function() {
    is(gBrowser.tabs.length, 2, "There are two tabs before cmd/ctrl + w is pressed");
    EventUtils.synthesizeKey("w", { accelKey: true }, contentWindow);
    is(gBrowser.tabs.length, 2, "There are two tabs after cmd/ctrl + w is pressed");

    gBrowser.removeTab(gBrowser.tabs[1]);
    test9();
  });
}

function test9() {
  let zoomLevel = ZoomManager.zoom;
  EventUtils.synthesizeKey("+", { accelKey: true }, contentWindow);
  is(ZoomManager.zoom, zoomLevel, "The zoom level remains unchanged after cmd/ctrl + + is pressed");

  EventUtils.synthesizeKey("-", { accelKey: true }, contentWindow);
  is(ZoomManager.zoom, zoomLevel, "The zoom level remains unchanged after cmd/ctrl + - is pressed");

  test10();
}

function test10() {
  is(gBrowser.tabs.length, 1, "There is one tab before cmd/ctrl + shift + a is pressed");
  // it would open about:addons on a new tab if it passes through the white list.
  EventUtils.synthesizeKey("a", { accelKey: true, shiftKey: true }, contentWindow);

  executeSoon(function() {
    is(gBrowser.tabs.length, 1, "There is still one tab after cmd/ctrl + shift + a is pressed");
    hideTabView(finish);
  })
}
