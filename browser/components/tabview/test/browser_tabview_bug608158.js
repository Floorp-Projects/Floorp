/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();
  newWindowWithTabView(onTabViewWindowLoaded);
}

function onTabViewWindowLoaded(win) {
  let contentWindow = win.TabView.getContentWindow();

  is(contentWindow.GroupItems.groupItems.length, 1, 
     "There is one group item on startup");
  is(win.gBrowser.tabs.length, 1, "There is one tab on startup");
  let groupItem = contentWindow.GroupItems.groupItems[0];

  hideGroupItem(groupItem, function () {
    hideTabView(() => {
      is(contentWindow.GroupItems.groupItems.length, 1, 
         "There is still one group item");
      isnot(groupItem, contentWindow.GroupItems.groupItems[0], 
            "The initial group item is not the same as the final group item");
      is(win.gBrowser.tabs.length, 1, "There is only one tab");
      ok(!win.TabView.isVisible(), "Tab View is hidden");
      promiseWindowClosed(win).then(finish);
    }, win);
  });
}
