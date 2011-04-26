/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let originalTab;
let orphanedTab;
let contentWindow;
let contentElement;

function test() {
  waitForExplicitFinish();

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
      orphanedTab = contentWindow.GroupItems.getOrphanedTabs()[0].tab;

      test2();
    });
  });

  // first click
  mouseClick(contentElement, 0);
  // second click
  mouseClick(contentElement, 0);
}

function test2() {
  let groupItem = createEmptyGroupItem(contentWindow, 300, 300, 200);
  is(groupItem.getChildren().length, 0, "The group is empty");

  hideTabView(function() {
    is(groupItem.getChildren().length, 1, "A tab is created inside the group");

    gBrowser.selectedTab = originalTab;
    gBrowser.removeTab(orphanedTab);
    gBrowser.removeTab(groupItem.getChildren()[0].tab);

    finish();
  });

  // first click
  mouseClick(groupItem.container, 0);
  // second click
  mouseClick(groupItem.container, 0);
}

function mouseClick(targetElement, buttonCode) {
  EventUtils.sendMouseEvent(
    { type: "mousedown", button: buttonCode }, targetElement, contentWindow);
  EventUtils.sendMouseEvent(
    { type: "mouseup", button: buttonCode }, targetElement, contentWindow);
}
