/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  ok(!TabView.isVisible(), "Tab View is hidden");
  showTabView(onTabViewShown);
}

function onTabViewShown() {
  let contentWindow = TabView.getContentWindow();
  let groupItems = contentWindow.GroupItems.groupItems;
  let groupItem = groupItems[0];

  is(groupItems.length, 1, "There is one group item on startup");

  hideGroupItem(groupItem, function () {
    whenTabViewIsHidden(function () {
      is(groupItems.length, 1, "There is still one group item");
      isnot(groupItem.id, groupItems[0].id, 
            "The initial group item is not the same as the final group item");
      is(gBrowser.tabs.length, 1, "There is only one tab");
      ok(!TabView.isVisible(), "Tab View is hidden");

      finish();
    });

    // create a new tab
    EventUtils.synthesizeKey("t", { accelKey: true });
  });
}
