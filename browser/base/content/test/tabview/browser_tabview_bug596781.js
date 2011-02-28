/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let newTab;

function test() {
  waitForExplicitFinish();

  newTab = gBrowser.addTab();
  gBrowser.pinTab(newTab);

  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  TabView.toggle();
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);
  ok(TabView.isVisible(), "Tab View is visible");

  let contentWindow = document.getElementById("tab-view").contentWindow;

  is(contentWindow.GroupItems.groupItems.length, 1, "Only one group exists"); 
  is(gBrowser.tabs.length, 2, "Only one tab exists");
  ok(newTab.pinned, "The original tab is pinned");

  let groupItem = contentWindow.GroupItems.groupItems[0];
  is(groupItem.$closeButton[0].style.display, "none", 
     "The close button is hidden");

  gBrowser.unpinTab(newTab);
  is(groupItem.$closeButton[0].style.display, "", 
     "The close button is visible");

  let onTabViewHidden = function() {
    window.removeEventListener("tabviewhidden", onTabViewHidden, false);
    gBrowser.removeTab(newTab);
    finish();
  }
  window.addEventListener("tabviewhidden", onTabViewHidden, false);
  TabView.toggle();
}
