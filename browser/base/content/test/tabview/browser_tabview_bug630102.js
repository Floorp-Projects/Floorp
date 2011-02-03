/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  let originalTab = gBrowser.visibleTabs[0];
  let newTab1 = gBrowser.addTab("about:blank", { skipAnimation: true });
  let newTab2 = gBrowser.addTab("about:blank", { skipAnimation: true });
  
  gBrowser.pinTab(newTab1);
  
  let contentWindow;

  let partOne = function() {
    window.removeEventListener("tabviewshown", partOne, false);

    contentWindow = document.getElementById("tab-view").contentWindow;
    is(contentWindow.GroupItems.groupItems.length, 1, "There is only one group item");

    let groupItem = contentWindow.GroupItems.groupItems[0];
    let tabItems = groupItem.getChildren();
    is(tabItems.length, 2, "There are two tab items in that group item");
    is(tabItems[0].tab, originalTab, "The first tab item is linked to the first tab");
    is(tabItems[1].tab, newTab2, "The second tab item is linked to the second tab");

    window.addEventListener("tabviewhidden", partTwo, false);
    TabView.toggle();
  };

  let partTwo = function() {
    window.removeEventListener("tabviewhidden", partTwo, false);

    gBrowser.unpinTab(newTab1);

    window.addEventListener("tabviewshown", partThree, false);
    TabView.toggle();
  };

  let partThree = function() {
    window.removeEventListener("tabviewshown", partThree, false);

    let tabItems = contentWindow.GroupItems.groupItems[0].getChildren();
    is(tabItems.length, 3, "There are three tab items in that group item");
    is(tabItems[0].tab, gBrowser.visibleTabs[0], "The first tab item is linked to the first tab");
    is(tabItems[1].tab, gBrowser.visibleTabs[1], "The second tab item is linked to the second tab");
    is(tabItems[2].tab, gBrowser.visibleTabs[2], "The third tab item is linked to the third tab");

    window.addEventListener("tabviewhidden", endGame, false);
    TabView.toggle();
  };

  let endGame = function() {
    window.removeEventListener("tabviewhidden", endGame, false);

    gBrowser.removeTab(newTab1);
    gBrowser.removeTab(newTab2);

    finish();
  };

  window.addEventListener("tabviewshown", partOne, false);
  TabView.toggle();
}
