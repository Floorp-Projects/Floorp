/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  showTabView(function () {
    is(gBrowser.tabs.length, 1, "There is only one tab");

    let tab = gBrowser.tabs[0];
    let tabItem = tab._tabViewTabItem;
    ok(tabItem.parent, "The tab item belongs to a group");
    let groupId = tabItem.parent.id;

    tab._tabViewTabItem.close();

    whenTabViewIsHidden(function() {
      // a new tab with group should be opened
      is(gBrowser.tabs.length, 1, "There is still one tab");
      isnot(gBrowser.selectedTab, tab, "The tab is different");

      tab = gBrowser.tabs[0];
      tabItem = tab._tabViewTabItem;
      ok(tabItem.parent, "This new tab item belongs to a group");

      is(tabItem.parent.id, groupId, "The group is different");

      finish();
    });
  });
}
