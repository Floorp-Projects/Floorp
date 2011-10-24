/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let newTab;

function test() {
  waitForExplicitFinish();

  newTab = gBrowser.addTab();

  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  TabView.toggle();
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);

  let contentWindow = document.getElementById("tab-view").contentWindow;
  is(contentWindow.GroupItems.groupItems.length, 1, "Has one group only");

  let tabItems = contentWindow.GroupItems.groupItems[0].getChildren();
  ok(tabItems.length, 2, "There are two tabItems in the group");

  is(tabItems[1].tab, newTab, "The second tabItem is linked to the new tab");

  EventUtils.sendMouseEvent({ type: "mousedown", button: 1 }, tabItems[1].container, contentWindow);
  EventUtils.sendMouseEvent({ type: "mouseup", button: 1 }, tabItems[1].container, contentWindow);

  ok(tabItems.length, 1, "There is only one tabItem in the group");

  let endGame = function() {
    window.removeEventListener("tabviewhidden", endGame, false);

    finish();
  };
  window.addEventListener("tabviewhidden", endGame, false);
  TabView.toggle();
}
