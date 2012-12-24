/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let contentWindow;
let appTab;
let originalTab;
let exitButton;

// ----------
function test() {
  waitForExplicitFinish();

  window.addEventListener("tabviewshown", onTabViewLoadedAndShown, false);
  TabView.toggle();
}

// ----------
function onTabViewLoadedAndShown() {
  window.removeEventListener("tabviewshown", onTabViewLoadedAndShown, false);
  ok(TabView.isVisible(), "Tab View is visible");

  contentWindow = document.getElementById("tab-view").contentWindow;

  // establish initial state
  is(contentWindow.GroupItems.groupItems.length, 1,
      "we start with one group (the default)");
  is(gBrowser.tabs.length, 1, "we start with one tab");
  originalTab = gBrowser.tabs[0];
  ok(!originalTab.pinned, "the original tab is not an app tab");

  // create an app tab
  appTab = gBrowser.loadOneTab("about:blank");
  is(gBrowser.tabs.length, 2, "we now have two tabs");
  gBrowser.pinTab(appTab);

  // verify that the normal tab is selected
  ok(originalTab.selected, "the normal tab is selected");

  // hit the exit button for the first time
  exitButton = contentWindow.document.getElementById("exit-button");
  ok(exitButton, "Exit button exists");

  window.addEventListener("tabviewhidden", onTabViewHiddenForNormalTab, false);
  EventUtils.sendMouseEvent({ type: "click" }, exitButton, contentWindow);
}

// ----------
function onTabViewHiddenForNormalTab() {
  window.removeEventListener("tabviewhidden", onTabViewHiddenForNormalTab, false);
  ok(!TabView.isVisible(), "Tab View is not visible");

  // verify that the normal tab is still selected
  ok(originalTab.selected, "the normal tab is still selected");

  // select the app tab
  gBrowser.selectedTab = appTab;
  ok(appTab.selected, "the app tab is now selected");

  // go back to tabview
  window.addEventListener("tabviewshown", onTabViewShown, false);
  TabView.toggle();
}

// ----------
function onTabViewShown() {
  window.removeEventListener("tabviewshown", onTabViewShown, false);
  ok(TabView.isVisible(), "Tab View is visible");

  // hit the exit button again
  window.addEventListener("tabviewhidden", onTabViewHiddenForAppTab, false);
  EventUtils.sendMouseEvent({ type: "click" }, exitButton, contentWindow);
}

// ----------
function onTabViewHiddenForAppTab() {
  window.removeEventListener("tabviewhidden", onTabViewHiddenForAppTab, false);
  ok(!TabView.isVisible(), "Tab View is not visible");

  // verify that the app tab is still selected
  ok(appTab.selected, "the app tab is still selected");

  // clean up
  gBrowser.selectedTab = originalTab;
  gBrowser.removeTab(appTab);

  is(gBrowser.tabs.length, 1, "we finish with one tab");
  ok(originalTab.selected,
      "we finish with the normal tab selected");
  ok(!TabView.isVisible(), "we finish with Tab View not visible");

  finish();
}
