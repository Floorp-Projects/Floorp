/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  let pinnedTab = gBrowser.addTab();
  gBrowser.pinTab(pinnedTab);

  registerCleanupFunction(function() {
    gBrowser.unpinTab(pinnedTab);
    while (gBrowser.tabs[1])
      gBrowser.removeTab(gBrowser.tabs[1]);
    hideTabView();
  });

  showTabView(function() {
    let cw = TabView.getContentWindow();
    let groupItemOne = cw.GroupItems.groupItems[0];
    let groupItemTwo = createGroupItemWithBlankTabs(window, 250, 250, 40, 1);

    is(cw.GroupItems.groupItems.length, 2, "Two group items");

    hideTabView(function() {
      gBrowser.selectedTab = pinnedTab;
      is(cw.GroupItems.getActiveGroupItem(), groupItemTwo, "Group two is active");
      is(gBrowser.selectedTab, pinnedTab, "Selected tab is the pinned tab");

      goToNextGroup();
      is(cw.GroupItems.getActiveGroupItem(), groupItemOne, "Group one is active");
      is(gBrowser.selectedTab, pinnedTab, "Selected tab is the pinned tab");

      finish();
    });
  });
}

