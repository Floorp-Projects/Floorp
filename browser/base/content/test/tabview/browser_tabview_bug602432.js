/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();
  newWindowWithTabView(onTabViewWindowLoaded);
}

let contentWindow = null;

function onTabViewWindowLoaded(win) {
  ok(win.TabView.isVisible(), "Tab View is visible");

  contentWindow = win.TabView.getContentWindow();

  // Preparation
  //
  let numTabs = 10;
  let groupItem = createGroupItemWithBlankTabs(win, 150, 150, 100,
    numTabs, false);

  // Ensure that group is stacked
  ok(groupItem.isStacked(), "Group item is stacked");

  // Force updates to be deferred
  contentWindow.TabItems.pausePainting();
  let children = groupItem.getChildren();
  is(children.length, numTabs, "Correct number of tabitems created");
   
  let leftToUpdate = numTabs;
  let testFunc = function(tabItem) {
    tabItem.removeSubscriber(tabItem, "updated");
    if (--leftToUpdate>0)
      return;
    // Now that everything is updated, compare update times.
    // All tabs in the group should have updated AFTER the first one.
    let earliest = children[0]._lastTabUpdateTime;
    for (let c=1; c<children.length; ++c)
      ok(earliest <= children[c]._lastTabUpdateTime,
        "Stacked group item update ("+children[c]._lastTabUpdateTime+") > first item ("+earliest+")");
    shutDown(win, groupItem);
  };

  for (let c=0; c<children.length; ++c) {
    let tabItem = children[c];
    tabItem.addSubscriber(tabItem, "updated", testFunc);
    contentWindow.TabItems.update(tabItem.tab);
  }

  // Let the update queue start again
  contentWindow.TabItems.resumePainting();
}

function shutDown(win, groupItem) {
  // Shut down
  let groupItemCount = contentWindow.GroupItems.groupItems.length;
  closeGroupItem(groupItem, function() {
    // check the number of groups.
    is(contentWindow.GroupItems.groupItems.length, --groupItemCount,
       "The number of groups is decreased by 1");
    let onTabViewHidden = function() {
      win.removeEventListener("tabviewhidden", onTabViewHidden, false);
      // assert that we're no longer in tab view
      ok(!TabView.isVisible(), "Tab View is hidden");
      win.close();
      ok(win.closed, "new window is closed");
      finish();
    };
    win.addEventListener("tabviewhidden", onTabViewHidden, false);
    win.TabView.toggle();
  });
}
