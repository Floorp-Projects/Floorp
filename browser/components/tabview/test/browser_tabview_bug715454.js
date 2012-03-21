/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let contentWindow;

function test() {
  waitForExplicitFinish();

  registerCleanupFunction(function() {
    while (gBrowser.tabs[1])
      gBrowser.removeTab(gBrowser.tabs[1]);
    hideTabView();
  });

  gBrowser.addTab("about:mozilla");
  showTabView(setup);
}


function setup() {
  let prefix = "setup: ";

  registerCleanupFunction(function() {
    let groupItem =  contentWindow.GroupItems.groupItem(groupItemTwoId);
    if (groupItem)
      closeGroupItem(groupItem);
  });

  contentWindow = TabView.getContentWindow();
  let groupItemOne = contentWindow.GroupItems.groupItems[0];

  contentWindow = TabView.getContentWindow();
  is(contentWindow.GroupItems.groupItems.length, 1,
    prefix + "There is only one group");

  is(groupItemOne.getChildren().length, 2,
    prefix + "The number of tabs in group one is 2");

  // Create a second group with a dummy page.
  let groupItemTwo = createGroupItemWithTabs(
    window, 300, 300, 310, ["about:blank"]);
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
   * At this point, focus is on group one. Let's switch to a tab with an URL
   * contained in group two and then open a new tab in group two after the
   * switch. The tab should be opened in group two and not in group one.
   */
  // Set the urlbar to include the moz-action.
  gURLBar.value = "moz-action:switchtab,about:mozilla";
  // Focus the urlbar so we can press enter.
  gURLBar.focus();
  // Press enter.
  EventUtils.synthesizeKey("VK_RETURN", {});

  // Open a new tab and make sure the tab is opened in the group one.
  EventUtils.synthesizeKey("t", { accelKey: true });

  // Check group two is active after a "switch to tab" action was executed and 
  // a new tab has been open.
  is(contentWindow.GroupItems.getActiveGroupItem(), groupItemOne, 
    prefix + "The group one is the active group");

  // Make sure the new tab is open in group one after the "switch to tab" action.
  is(groupItemOne.getChildren().length, 3,
    prefix + "The number of children in group one is 3");

  // Verify there's only one tab in group two after the "switch to tab" action.
  is(groupItemTwo.getChildren().length, 1,
    prefix + "The number of children in group two is 1");

  finish();
}
