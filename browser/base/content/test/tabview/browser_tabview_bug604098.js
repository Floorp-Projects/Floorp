/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let originalTab;
let orphanedTab;
let contentWindow;

function test() {
  waitForExplicitFinish();

  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  TabView.show();
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);

  contentWindow = document.getElementById("tab-view").contentWindow;
  originalTab = gBrowser.visibleTabs[0];

  test1();
}

function test1() {
  is(contentWindow.GroupItems.getOrphanedTabs().length, 0, "No orphaned tabs");

  let onTabViewHidden = function() {
    window.removeEventListener("tabviewhidden", onTabViewHidden, false);

    let onTabViewShown = function() {
      window.removeEventListener("tabviewshown", onTabViewShown, false);

      is(contentWindow.GroupItems.getOrphanedTabs().length, 1, 
         "An orphaned tab is created");
      orphanedTab = contentWindow.GroupItems.getOrphanedTabs()[0].tab;

      test2();
    };
    window.addEventListener("tabviewshown", onTabViewShown, false);
    TabView.show();
  };
  window.addEventListener("tabviewhidden", onTabViewHidden, false);

  // first click
  EventUtils.sendMouseEvent(
    { type: "mousedown" }, contentWindow.document.getElementById("content"), 
    contentWindow);
  EventUtils.sendMouseEvent(
    { type: "mouseup" }, contentWindow.document.getElementById("content"), 
    contentWindow);
  // second click
  EventUtils.sendMouseEvent(
    { type: "mousedown" }, contentWindow.document.getElementById("content"), 
    contentWindow);
  EventUtils.sendMouseEvent(
    { type: "mouseup" }, contentWindow.document.getElementById("content"), 
    contentWindow);
}

function test2() {
  let groupItem = createEmptyGroupItem(contentWindow, 300, 300, 200);
  is(groupItem.getChildren().length, 0, "The group is empty");

  let onTabViewHidden = function() {
    window.removeEventListener("tabviewhidden", onTabViewHidden, false);

    is(groupItem.getChildren().length, 1, "A tab is created inside the group");
    
    gBrowser.selectedTab = originalTab;
    gBrowser.removeTab(orphanedTab);
    gBrowser.removeTab(groupItem.getChildren()[0].tab);

    finish();
  };
  window.addEventListener("tabviewhidden", onTabViewHidden, false);

  // first click
  EventUtils.sendMouseEvent(
    { type: "mousedown" }, groupItem.container, contentWindow);
  EventUtils.sendMouseEvent(
    { type: "mouseup" }, groupItem.container, contentWindow);
  // second click
  EventUtils.sendMouseEvent(
    { type: "mousedown" }, groupItem.container, contentWindow);
  EventUtils.sendMouseEvent(
    { type: "mouseup" }, groupItem.container, contentWindow);
}
