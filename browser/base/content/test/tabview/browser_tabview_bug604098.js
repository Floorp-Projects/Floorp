/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let originalTab;
let orphanedTab;
let contentWindow;
let contentElement;

function test() {
  waitForExplicitFinish();

  registerCleanupFunction(function() {
    if (gBrowser.tabs.length > 1)
      gBrowser.removeTab(gBrowser.tabs[1]);
    hideTabView(function() {});
  });

  showTabView(function() {
    contentWindow = TabView.getContentWindow();
    contentElement = contentWindow.document.getElementById("content");
    originalTab = gBrowser.visibleTabs[0];
    test1();
  });
}

function test1() {
  is(contentWindow.GroupItems.getOrphanedTabs().length, 0, "No orphaned tabs");

  whenTabViewIsHidden(function() {
    showTabView(function() {
      is(contentWindow.GroupItems.getOrphanedTabs().length, 1,
         "An orphaned tab is created");
      hideTabView(function() {
        gBrowser.selectedTab = originalTab;
        finish();
      });
    });
  });

  // first click
  mouseClick(contentElement, 0);
  // second click
  mouseClick(contentElement, 0);
}

function mouseClick(targetElement, buttonCode) {
  EventUtils.sendMouseEvent(
    { type: "mousedown", button: buttonCode }, targetElement, contentWindow);
  EventUtils.sendMouseEvent(
    { type: "mouseup", button: buttonCode }, targetElement, contentWindow);
}

