/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  TabView.toggle();
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);

  let contentWindow = document.getElementById("tab-view").contentWindow;

  is(contentWindow.GroupItems.groupItems.length, 1, 
     "There is one group item on startup");
  is(gBrowser.tabs.length, 1, "There is one tab on startup");
  let groupItem = contentWindow.GroupItems.groupItems[0];

  hideGroupItem(groupItem, function () {
    let onTabViewHidden = function() {
      window.removeEventListener("tabviewhidden", onTabViewHidden, false);
      is(contentWindow.GroupItems.groupItems.length, 1, 
         "There is still one group item");
      isnot(groupItem, contentWindow.GroupItems.groupItems[0], 
            "The initial group item is not the same as the final group item");
      is(gBrowser.tabs.length, 1, "There is only one tab");
      ok(!TabView.isVisible(), "Tab View is hidden");
      finish();
    };
    window.addEventListener("tabviewhidden", onTabViewHidden, false);

    TabView.hide();
  });
}
