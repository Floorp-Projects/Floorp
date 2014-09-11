/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let contentWindow;
let pinnedTab;

function test() {
  waitForExplicitFinish();

  registerCleanupFunction(function() {
    while (gBrowser.tabs[1])
      gBrowser.removeTab(gBrowser.tabs[1]);
    hideTabView();
  });

  pinnedTab = gBrowser.addTab("about:blank");
  gBrowser.pinTab(pinnedTab);
  ok(pinnedTab.pinned, "Tab 1 is pinned");

  gBrowser.addTab("about:mozilla");
  showTabView(setup);
}

function setup() {
  let prefix = "setup: ";
  
  registerCleanupFunction(function() {
    let groupItem = contentWindow.GroupItems.groupItem(groupItemTwoId);
    if (groupItem)
      closeGroupItem(groupItem);
  });

  contentWindow = TabView.getContentWindow();
  let groupItemOne = contentWindow.GroupItems.groupItems[0];

  is(contentWindow.GroupItems.groupItems.length, 1,
    prefix + "There is only one group");

  is(groupItemOne.getChildren().length, 2,
    prefix + "The number of tabs in group one is 2");

  // Create a second group with a dummy page.
  let groupItemTwo =
    createGroupItemWithTabs(window, 300, 300, 310, ["about:blank"]);
  let groupItemTwoId = groupItemTwo.id;

  // Add a new tab to the second group, from where we will execute the switch
  // to tab.
  groupItemTwo.newTab("about:blank");

  is(contentWindow.GroupItems.getActiveGroupItem(), groupItemTwo, 
     prefix + "The group two is the active group");
  
  is(contentWindow.UI.getActiveTab(), groupItemTwo.getChild(1), 
     prefix + "The second tab item in group two is active");

  hideTabView(function () { switchToURL(groupItemOne, groupItemTwo) } );
}


function switchToURL(groupItemOne, groupItemTwo) {
  let prefix = "after switching: ";

  /**
   * At this point, focus is on group two. Let's switch to a tab with an URL
   * contained in group one and then go to the pinned tab after the
   * switch. The selected group should be group one.
   */
  // Set the urlbar to include the moz-action.
  gURLBar.value = "moz-action:switchtab,about:mozilla";
  // Focus the urlbar so we can press enter.
  gURLBar.focus();
  // Press enter.
  EventUtils.synthesizeKey("VK_RETURN", {});

  // Focus on the app tab.
  EventUtils.synthesizeKey("1", { accelKey: true });

  // Check group one is active after a "switch to tab" action was executed and 
  // the app tab receives focus.
  is(contentWindow.GroupItems.getActiveGroupItem(), groupItemOne, 
    prefix + "The group one is the active group");

  is(groupItemOne.getChildren().length, 2,
    prefix + "The number of tabs in group one is 2");

  is(groupItemTwo.getChildren().length, 1,
    prefix + "The number of tabs in group two is 1");

  gBrowser.removeTab(pinnedTab);
  finish();
}