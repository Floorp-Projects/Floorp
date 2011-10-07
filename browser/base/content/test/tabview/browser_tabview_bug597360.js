/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(function (win) {
    registerCleanupFunction(function () win.close());

    let cw = win.TabView.getContentWindow();
    let groupItems = cw.GroupItems.groupItems;
    let groupItem = groupItems[0];

    is(groupItems.length, 1, "There is one group item on startup");

    whenTabViewIsHidden(function () {
      is(groupItems.length, 1, "There is still one group item");
      isnot(groupItem.id, groupItems[0].id, 
            "The initial group item is not the same as the final group item");
      is(win.gBrowser.tabs.length, 1, "There is only one tab");

      finish();
    }, win);

    hideGroupItem(groupItem, function () {
      // create a new tab
      EventUtils.synthesizeKey("t", { accelKey: true }, cw);
    });
  });
}
