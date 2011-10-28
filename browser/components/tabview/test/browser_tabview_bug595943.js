/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  // Show TabView
  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  TabView.toggle();
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);
  ok(TabView.isVisible(), "Tab View is visible");

  let contentWindow = document.getElementById("tab-view").contentWindow;

  // establish initial state
  is(contentWindow.GroupItems.groupItems.length, 1, "we start with one group (the default)"); 
  is(gBrowser.tabs.length, 1, "we start with one tab");

  let originalTab = gBrowser.tabs[0];
  ok(contentWindow.GroupItems.groupItems[0]._children[0].tab == originalTab, 
    "the original tab is in the original group");
      
  // create a second group 
  let box = new contentWindow.Rect(20, 20, 180, 180);
  let groupItem = new contentWindow.GroupItem([], { bounds: box });
  is(contentWindow.GroupItems.groupItems.length, 2, "we now have two groups");
  contentWindow.UI.setActive(groupItem);
  
  // create a second tab
  let normalXulTab = gBrowser.loadOneTab("about:blank");
  is(gBrowser.tabs.length, 2, "we now have two tabs");
  is(groupItem._children.length, 1, "the new tab was added to the group");
  
  // create a third tab
  let appXulTab = gBrowser.loadOneTab("about:blank");
  is(gBrowser.tabs.length, 3, "we now have three tabs");
  gBrowser.pinTab(appXulTab);
  is(groupItem._children.length, 1, "the app tab is not in the group");
  
  // We now have two groups with one tab each, plus an app tab.
  // Click into one of the tabs, close it and make sure we don't go back to Tab View.
  function onTabViewHidden() {
    window.removeEventListener("tabviewhidden", onTabViewHidden, false);
    ok(!TabView.isVisible(), "Tab View is hidden because we clicked on the app tab");
  
    // Remove the tab we're looking at.
    gBrowser.removeTab(normalXulTab);  
  
    // Make sure we haven't returned to TabView; this is the crux of this test
    ok(!TabView.isVisible(), "Tab View remains hidden");
  
    // clean up
    gBrowser.selectedTab = originalTab;
    
    gBrowser.unpinTab(appXulTab);
    gBrowser.removeTab(appXulTab);

    ok(groupItem.closeIfEmpty(), "the second group was empty");

    // Verify ending state
    is(gBrowser.tabs.length, 1, "we finish with one tab");
    is(contentWindow.GroupItems.groupItems.length, 1,
       "we finish with one group");
    ok(!TabView.isVisible(), "we finish with Tab View hidden");
      
    finish();
  }

  window.addEventListener("tabviewhidden", onTabViewHidden, false);
  EventUtils.sendMouseEvent({ type: "mousedown" }, normalXulTab._tabViewTabItem.container, contentWindow);
  EventUtils.sendMouseEvent({ type: "mouseup" }, normalXulTab._tabViewTabItem.container, contentWindow);
}
